/* World Without Answers — forge_manifest.c
   Append-only content-hash database (ninja_log / tup style).
   Record: [path_hash u64][content_hash u64][cmd_hash u64][mtime i64][size i64]
   Latest record per path wins. Fast path: if mtime+size match the
   stored record, reuse stored content hash without re-reading
   (CloudBuild-style trust after verification). */
#include <forge_internal.h>
#include <stdmem.h>
#include <stdstr.h>
#include <stdos.h>
#include <stdhash.h>
#include <stdio.h>

#define FORGE_MANIFEST_PATH "bin_obj/forge_manifest.bin"
#define FORGE_MANIFEST_MAGIC 0x464F5247454D414Eull /* "FORGEMAN" */

typedef struct {
    forge_manifest_rec_t* recs;
    usize count, cap;
} forge_db_t;

static forge_db_t g_db;

void forge_manifest_load(void) {
    wwa_memset(&g_db, 0, sizeof(g_db));
    g_db.cap = 4096;
    g_db.recs = (forge_manifest_rec_t*)wwa_os_alloc(
        sizeof(forge_manifest_rec_t) * g_db.cap);
    if (!wwa_os_file_exists(FORGE_MANIFEST_PATH)) return;
    i32 fd = wwa_os_file_open(FORGE_MANIFEST_PATH, WWA_OS_FILE_READ);
    if (fd < 0) return;
    i64 size = wwa_os_file_size(FORGE_MANIFEST_PATH);
    if (size < (i64)sizeof(u64)) { wwa_os_file_close(fd); return; }
    u8* buf = (u8*)wwa_os_alloc((usize)size);
    /* read via fd loop */
    i64 off = 0;
    while (off < size) {
        i32 r = wwa_os_read(fd, buf + off, (usize)(size - off));
        if (r <= 0) break;
        off += r;
    }
    wwa_os_file_close(fd);
    u64 magic; wwa_memmove(&magic, buf, 8);
    if (magic != FORGE_MANIFEST_MAGIC) { wwa_os_free(buf, (usize)size); return; }
    usize nrec = ((usize)size - 8) / sizeof(forge_manifest_rec_t);
    for (usize i = 0; i < nrec; i++) {
        forge_manifest_rec_t rec;
        wwa_memmove(&rec, buf + 8 + i * sizeof(rec), sizeof(rec));
        if (g_db.count >= g_db.cap) {
            usize ncap = g_db.cap * 2;
            forge_manifest_rec_t* nr = (forge_manifest_rec_t*)wwa_os_alloc(
                sizeof(forge_manifest_rec_t) * ncap);
            wwa_memmove(nr, g_db.recs, sizeof(forge_manifest_rec_t) * g_db.count);
            wwa_os_free(g_db.recs, sizeof(forge_manifest_rec_t) * g_db.cap);
            g_db.recs = nr; g_db.cap = ncap;
        }
        g_db.recs[g_db.count++] = rec;
    }
    wwa_os_free(buf, (usize)size);
}

/* linear scan from end — fine for <100k records; map later if needed */
i32 forge_manifest_get(u64 path_hash, forge_manifest_rec_t* rec) {
    for (usize i = g_db.count; i > 0; i--) {
        if (g_db.recs[i-1].path_hash == path_hash) {
            *rec = g_db.recs[i-1];
            return 1;
        }
    }
    return 0;
}

i32 forge_manifest_dbg_count(void) { return (i32)g_db.count; }
u64 forge_manifest_dbg_rec0(void) { return g_db.count ? g_db.recs[0].path_hash : 0; }

void forge_manifest_append(const char* path, u64 path_hash,
                           u64 content_hash, u64 cmd_hash,
                           i64 mtime, i64 size)
{
    (void)path;
    if (g_db.count >= g_db.cap) {
        usize ncap = g_db.cap ? g_db.cap * 2 : 4096;
        forge_manifest_rec_t* nr = (forge_manifest_rec_t*)wwa_os_alloc(
            sizeof(forge_manifest_rec_t) * ncap);
        wwa_memmove(nr, g_db.recs, sizeof(forge_manifest_rec_t) * g_db.count);
        if (g_db.recs) wwa_os_free(g_db.recs, sizeof(forge_manifest_rec_t) * g_db.cap);
        g_db.recs = nr; g_db.cap = ncap;
    }
    forge_manifest_rec_t rec;
    rec.path_hash = path_hash;
    rec.content_hash = content_hash;
    rec.cmd_hash = cmd_hash;
    rec.mtime = mtime;
    rec.size = size;
    g_db.recs[g_db.count++] = rec;

    /* append to disk */
    i32 fd = wwa_os_file_open(FORGE_MANIFEST_PATH,
        WWA_OS_FILE_WRITE | WWA_OS_FILE_CREATE | WWA_OS_FILE_APPEND);
    if (fd < 0) return;
    if (wwa_os_file_size(FORGE_MANIFEST_PATH) == 0) {
        u64 magic = FORGE_MANIFEST_MAGIC;
        wwa_os_write(fd, &magic, 8);
    }
    wwa_os_write(fd, &rec, sizeof(rec));
    wwa_os_file_close(fd);
}

/* stat fast-path: reuse stored hash when mtime+size unchanged */
u64 forge_manifest_file_hash(const char* path) {
    u64 ph = wwa_hash_str(path);
    i64 mtime = wwa_os_file_mtime(path);
    i64 size = wwa_os_file_size(path);
    forge_manifest_rec_t rec;
    if (mtime >= 0 && forge_manifest_get(ph, &rec) &&
        rec.mtime == mtime && rec.size == size && rec.content_hash != 0) {
        return rec.content_hash; /* trusted fast path */
    }
    /* full read + wyhash */
    if (!wwa_os_file_exists(path)) return 0;
    i32 fd = wwa_os_file_open(path, WWA_OS_FILE_READ);
    if (fd < 0) return 0;
    i64 fsize = wwa_os_file_size(path);
    if (fsize < 0) { wwa_os_file_close(fd); return 0; }
    static u8 chunk[65536];
    wwa_hash_state_t hs;
    wwa_hash_init(&hs, WWA_HASH_SEED);
    i64 left = fsize;
    while (left > 0) {
        usize want = left > 65536 ? 65536 : (usize)left;
        i32 r = wwa_os_read(fd, chunk, want);
        if (r <= 0) break;
        wwa_hash_update(&hs, chunk, (usize)r);
        left -= r;
    }
    wwa_os_file_close(fd);
    return wwa_hash_final(&hs);
}

void forge_manifest_compact(void) {
    /* keep only latest record per path_hash, rewrite file */
    if (g_db.count == 0) return;
    /* in-place dedup: mark older duplicates with path_hash=0 */
    for (usize i = 0; i < g_db.count; i++) {
        for (usize j = i + 1; j < g_db.count; j++) {
            if (g_db.recs[j].path_hash != 0 &&
                g_db.recs[j].path_hash == g_db.recs[i].path_hash)
                g_db.recs[i].path_hash = 0; /* older dup killed */
        }
    }
    usize w = 0;
    for (usize i = 0; i < g_db.count; i++) {
        if (g_db.recs[i].path_hash != 0) g_db.recs[w++] = g_db.recs[i];
    }
    g_db.count = w;
    i32 fd = wwa_os_file_open(FORGE_MANIFEST_PATH,
        WWA_OS_FILE_WRITE | WWA_OS_FILE_CREATE | WWA_OS_FILE_TRUNC);
    if (fd < 0) return;
    u64 magic = FORGE_MANIFEST_MAGIC;
    wwa_os_write(fd, &magic, 8);
    wwa_os_write(fd, g_db.recs, sizeof(forge_manifest_rec_t) * g_db.count);
    wwa_os_file_close(fd);
}

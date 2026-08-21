/* World Without Answers — forge_sched.c
   N-worker list scheduler with critical-path priority,
   content-hash up-to-dateness and CloudBuild-style early cutoff.

   Termination: graph pre-validated acyclic (Kahn), so while unfinished
   work exists and no node is RUNNING, some node has all deps final ->
   READY nonempty. Each edge releases at most once per direction.
   Complexity O(V+E) scheduling overhead + command time. */
#include <forge_internal.h>
#include <stdmem.h>
#include <stdstr.h>
#include <stdos.h>
#include <stdthread.h>
#include <stdtime.h>
#include <stdio.h>

f64 g_forge_build_ms = 0;

typedef struct {
    forge_graph_t* g;
    wwa_mutex_t    mtx;
    wwa_cond_t     cv;
    i32            running;
    i32            finished;
    i32            failed;
    i32            n_clean, n_built, n_cut, n_skipped_cycle;
    i32            executed;
    i32            total;
    i32            alive;
} forge_sched_t;

static void forge_status(forge_sched_t* s, const char* verb, const char* path) {
    wwa_printf("[%d/%d] %s %s\n", s->executed, s->total, verb, path);
}

static i32 forge_deps_unchanged(forge_sched_t* s, forge_node_t* n) {
    forge_manifest_rec_t drec;
    for (i32 k = 0; k < n->dep_count; k++) {
        forge_node_t* d = &s->g->nodes[n->deps[k]];
        /* dep's current-pass value */
        u64 cur = d->new_hash;
        if (cur == 0) return 0; /* dep produced nothing / failed upstream */
        /* what this node last saw for dep */
        if (!forge_manifest_get(d->out_hash, &drec)) return 0;
        if (drec.content_hash != cur) return 0;
    }
    return 1;
}

static void forge_mkdirs(const char* path) {
    char buf[512];
    usize len = wwa_strlen(path);
    if (len >= sizeof(buf)) return;
    wwa_memmove(buf, path, len + 1);
    for (usize i = 1; i < len; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            buf[i] = 0;
            wwa_os_dir_create(buf);
            buf[i] = '/';
        }
    }
}

/* Run argv with the output path redirected to <out>.tmp, then atomically
   move it into place. Windows cannot overwrite a running executable's
   image file, so we rename the old one aside first (renaming a running
   image IS allowed) — this makes forge able to relink itself. */
static i32 forge_execute_cmd(forge_node_t* n) {
    char tmp[600];
    usize olen = wwa_strlen(n->out);
    if (olen + 5 >= sizeof(tmp)) return -1;
    wwa_memmove(tmp, n->out, olen);
    wwa_memmove(tmp + olen, ".tmp", 5);

    const char** args = (const char**)wwa_os_alloc(sizeof(char*) * (usize)(n->argc + 1));
    for (i32 k = 0; k < n->argc; k++) {
        if (wwa_strcmp(n->argv[k], n->out) == 0) args[k] = tmp;
        else args[k] = n->argv[k];
    }
    args[n->argc] = NULL;
    i32 code = -1;
    i32 rc = wwa_os_spawn(args[0], args, &code);
    wwa_os_free(args, sizeof(char*) * (usize)(n->argc + 1));
    if (rc != 0 || code != 0) {
        wwa_os_file_delete(tmp); /* best effort */
        return -1;
    }

    /* publish: try atomic replace first; if out is a locked running
       image (self-hosting), move it aside under a free .oldN name */
    if (wwa_os_rename_ex(tmp, n->out, 1) != 0) {
        char oldp[620];
        if (olen + 12 >= sizeof(oldp)) { wwa_os_file_delete(tmp); return -1; }
        wwa_memmove(oldp, n->out, olen);
        for (i32 tries = 1; tries <= 99; tries++) {
            oldp[olen] = '.';
            usize n2 = olen + 1;
            u32 v = (u32)tries;
            char nb[4];
            i32 nd = 0;
            if (v >= 100) nb[nd++] = (char)('0' + v / 100);
            if (v >= 10) nb[nd++] = (char)('0' + (v / 10) % 10);
            nb[nd++] = (char)('0' + v % 10);
            nb[nd] = 0;
            wwa_memmove(oldp + n2, "old", 3);
            n2 += 3;
            wwa_memmove(oldp + n2, nb, (usize)nd + 1);
            if (wwa_os_rename_ex(n->out, oldp, 1) == 0) break;
            if (wwa_os_file_exists(n->out)) continue;
            break;
        }
        if (wwa_os_rename_ex(tmp, n->out, 1) != 0) {
            wwa_os_file_delete(tmp);
            return -1;
        }
    }
    return 0;
}

/* returns final state for node */
static i32 forge_process(forge_sched_t* s, forge_node_t* n) {
    forge_manifest_rec_t orec;
    i32 have_orec = forge_manifest_get(n->out_hash, &orec);

    /* --- source file --- */
    if (!n->argv) {
        u64 h = forge_manifest_file_hash(n->out);
        n->new_hash = h;
        if (h == 0) {
            wwa_printf("forge: missing source %s\n", n->out);
            return FORGE_ST_FAILED;
        }
        if (have_orec && orec.content_hash == h) return FORGE_ST_CLEAN;
        i64 mt = wwa_os_file_mtime(n->out);
        forge_manifest_append(n->out, n->out_hash, h, 0, mt,
                              wwa_os_file_size(n->out));
        return FORGE_ST_BUILT;
    }

    /* --- command node: try to skip --- */
    if (have_orec && orec.cmd_hash == n->cmd_hash && orec.content_hash != 0 &&
        wwa_os_file_exists(n->out)) {
        u64 out_now = forge_manifest_file_hash(n->out); /* stat fast path */
        if (out_now == orec.content_hash && forge_deps_unchanged(s, n)) {
            n->new_hash = out_now;
            return FORGE_ST_CLEAN;
        }
        wwa_printf("forge: rebuild %s (out=%d deps=%d)\n", n->out,
                   out_now == orec.content_hash ? 1 : 0,
                   forge_deps_unchanged(s, n));
    } else if (n->argv) {
        static i32 dbg_done = 0;
        if (!dbg_done) {
            dbg_done = 1;
            wwa_printf("forge: dbcount=%d rec0=%016llx query=%016llx (%s)\n",
                       forge_manifest_dbg_count(),
                       (unsigned long long)forge_manifest_dbg_rec0(),
                       (unsigned long long)n->out_hash, n->out);
        }
        wwa_printf("forge: rebuild %s (orec=%d cmd=%d chash=%d exists=%d)\n",
                   n->out, have_orec,
                   have_orec ? orec.cmd_hash == n->cmd_hash : -1,
                   have_orec ? orec.content_hash != 0 : -1,
                   wwa_os_file_exists(n->out));
    }

    /* --- rebuild --- */
    f64 t0 = wwa_time_us() / 1000.0;
    forge_mkdirs(n->out);
    if (forge_execute_cmd(n) != 0) {
        wwa_printf("forge: FAILED %s\n", n->out);
        return FORGE_ST_FAILED;
    }
    f64 dt = wwa_time_us() / 1000.0 - t0;
    n->weight_ms = dt > 0.5 ? dt : 0.5;

    u64 out_new = forge_manifest_file_hash(n->out); /* full rehash (mtime moved) */
    n->new_hash = out_new;
    i64 mt = wwa_os_file_mtime(n->out);
    forge_manifest_append(n->out, n->out_hash, out_new, n->cmd_hash, mt,
                          wwa_os_file_size(n->out));

    /* early cutoff: value identical to what dependents last consumed */
    if (have_orec && orec.content_hash == out_new) return FORGE_ST_CUT;
    return FORGE_ST_BUILT;
}

static void forge_fail_cascade(forge_sched_t* s, i32 id) {
    /* iterative DFS marking downstream FAILED */
    i32* stack = (i32*)wwa_os_alloc(sizeof(i32) * (usize)s->g->node_count);
    i32 sp = 0;
    stack[sp++] = id;
    while (sp > 0) {
        i32 u = stack[--sp];
        forge_node_t* n = &s->g->nodes[u];
        for (i32 k = 0; k < n->rdep_count; k++) {
            forge_node_t* r = &s->g->nodes[n->rdeps[k]];
            if (r->state == FORGE_ST_PENDING || r->state == FORGE_ST_READY) {
                r->state = FORGE_ST_FAILED;
                s->finished++;
                s->failed++;
                stack[sp++] = n->rdeps[k];
            }
        }
    }
    wwa_os_free(stack, sizeof(i32) * (usize)s->g->node_count);
}

static void forge_worker(forge_sched_t* s) {
    wwa_mutex_lock(&s->mtx);
    for (;;) {
        /* promote newly-ready nodes and pick highest cprio */
        i32 best = -1;
        f64 best_prio = -1;
        for (i32 i = 0; i < s->g->node_count; i++) {
            forge_node_t* n = &s->g->nodes[i];
            if (n->state == FORGE_ST_PENDING && n->dep_left == 0)
                n->state = FORGE_ST_READY;
            if (n->state == FORGE_ST_READY && n->cprio > best_prio) {
                best_prio = n->cprio;
                best = i;
            }
        }
        if (best < 0) {
            if (s->running == 0) break;   /* quiescent: done */
            wwa_cond_wait(&s->cv, &s->mtx);
            continue;
        }
        forge_node_t* n = &s->g->nodes[best];
        if (n->out == NULL) { /* defensive: should be impossible */
            n->state = FORGE_ST_FAILED;
            s->finished++;
            s->failed++;
            continue;
        }
        n->state = FORGE_ST_RUNNING;
        s->running++;
        s->executed++;
        if (n->argv) forge_status(s, "cc", n->out);
        wwa_mutex_unlock(&s->mtx);

        i32 st = forge_process(s, n);

        wwa_mutex_lock(&s->mtx);
        s->running--;
        n->state = st;
        s->finished++;
        if (st == FORGE_ST_FAILED) {
            s->failed++;
            forge_fail_cascade(s, best);
        } else if (st == FORGE_ST_BUILT) s->n_built++;
        else if (st == FORGE_ST_CUT) s->n_cut++;
        else s->n_clean++;

        for (i32 k = 0; k < n->rdep_count; k++) {
            forge_node_t* r = &s->g->nodes[n->rdeps[k]];
            r->dep_left--;
        }
        wwa_cond_broadcast(&s->cv);
    }
    s->alive--;
    wwa_cond_broadcast(&s->cv);
    wwa_mutex_unlock(&s->mtx);
}

static void forge_worker_thunk(void* arg) {
    forge_worker((forge_sched_t*)arg);
}

i32 forge_sched_run(forge_graph_t* g, i32 workers) {
    forge_sched_t s;
    wwa_memset(&s, 0, sizeof(s));
    s.g = g;
    s.total = g->node_count;
    wwa_mutex_init(&s.mtx);
    wwa_cond_init(&s.cv);

    f64 t0 = wwa_time_us();
    if (workers <= 1) {
        forge_worker(&s);
    } else {        s.alive = workers;
        wwa_thread_t th[64];
        i32 spawned = 0;
        for (i32 i = 0; i < workers && i < 64; i++) {
            if (wwa_thread_spawn(&th[i], forge_worker_thunk, &s) == 0)
                spawned++;
            else break;
        }
        /* join */
        wwa_mutex_lock(&s.mtx);
        while (s.alive > 0) {
            wwa_mutex_unlock(&s.mtx);
            wwa_thread_join(th[--spawned]);
            wwa_mutex_lock(&s.mtx);
        }
        wwa_mutex_unlock(&s.mtx);
    }
    g_forge_build_ms = (wwa_time_us() - t0) / 1000.0;

    wwa_mutex_destroy(&s.mtx);
    wwa_cond_destroy(&s.cv);

    wwa_printf("forge: %d nodes | built %d, cut %d (early cutoff), clean %d, failed %d | %.1f ms\n",
               g->node_count, s.n_built, s.n_cut, s.n_clean, s.failed,
               g_forge_build_ms);
    return s.failed;
}

/* World Without Answers — forge_internal.h — shared between forge_*.c */
#ifndef WWA_FORGE_INTERNAL_H
#define WWA_FORGE_INTERNAL_H

#include <forge.h>
#include <stdtype.h>

/* node states */
#define FORGE_ST_PENDING 0
#define FORGE_ST_READY   1
#define FORGE_ST_RUNNING 2
#define FORGE_ST_CLEAN   3   /* verified up-to-date */
#define FORGE_ST_BUILT   4   /* rebuilt, output changed */
#define FORGE_ST_FAILED  5
#define FORGE_ST_CUT     6   /* rebuilt, output identical (early cutoff) */

/* --- research evidence hierarchy (ranked weakest..strongest claim support) --- */
#define WWA_ER_LEVEL_A 0  /* mechanically obvious invariant from code/architecture */
#define WWA_ER_LEVEL_B 1  /* compiler-verified property or generated-code inspection */
#define WWA_ER_LEVEL_C 2  /* repeatable microbenchmark result */
#define WWA_ER_LEVEL_D 3  /* repeatable integrated benchmark result */
#define WWA_ER_LEVEL_E 4  /* profile/counter evidence explaining the result */
#define WWA_ER_LEVEL_F 5  /* external paper / production precedent */
#define WWA_ER_LEVEL_G 6  /* untested hypothesis / intuition */

/* Highest evidence level any promoted forge claim currently rests on. */
#define WWA_RESEARCH_LEVEL WWA_ER_LEVEL_D

#if defined(__GNUC__) || defined(__clang__)
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)
#elif defined(_MSC_VER)
#include <vadefs.h>
#else
#error "stdarg: no varargs support for this compiler"
#endif

typedef struct forge_node {
    char*  out;
    u64    out_hash;      /* path hash */
    i32*   deps;
    i32    dep_count;
    char** argv;
    i32    argc;
    u64    cmd_hash;
    i32    dep_left;
    i32*   rdeps;
    i32    rdep_count;
    i32    state;
    f64    weight_ms;
    f64    cprio;
    /* runtime */
    u64    new_hash;      /* content hash after this build pass */
} forge_node_t;

struct forge_graph {
    forge_node_t* nodes;
    i32           node_count;
    i32           cap;
    i32*          map;
    usize         map_cap;
};

/* --- manifest (forge_manifest.c) ---
   sig_hash is the *build signature* a command node was produced from:
   mix(cmd_hash, content hash of every dependency in declaration order).
   Recording the signature — not the dependency hashes themselves — is what
   makes the up-to-date check sound: a dependency rewrites its own record
   during the same pass, so comparing against the dependency's record can
   never detect a change (it always sees the new value). Source nodes leave
   sig_hash at 0. */
typedef struct {
    u64 path_hash;
    u64 content_hash;
    u64 sig_hash;
    i64 mtime;
    i64 size;
} forge_manifest_rec_t;

void forge_manifest_load(void);
void forge_manifest_append(const char* path, u64 path_hash,
                           u64 content_hash, u64 sig_hash,
                           i64 mtime, i64 size);
/* lookup: fills rec, returns 1 if found (latest record wins) */
i32  forge_manifest_get(u64 path_hash, forge_manifest_rec_t* rec);
i32  forge_manifest_dbg_count(void);
u64  forge_manifest_dbg_rec0(void);
u64  forge_manifest_file_hash(const char* path); /* stat fast-path + wyhash */
void forge_manifest_compact(void);

/* --- scheduler (forge_sched.c) --- */
i32 forge_sched_run(forge_graph_t* g, i32 workers);
extern f64 g_forge_build_ms;
extern i32 g_forge_n_built, g_forge_n_cut, g_forge_n_clean, g_forge_n_failed;

#endif

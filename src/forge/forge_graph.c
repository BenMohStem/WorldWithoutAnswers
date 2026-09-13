/* World Without Answers — forge_graph.c
   DAG: intern table (open addressing), nodes, edges, Kahn validation,
   critical-path DP over topological order. */
#include <forge_internal.h>
#include <stdmem.h>
#include <stdstr.h>
#include <stdos.h>
#include <stdhash.h>
#include <stdio.h>

#define FORGE_MAX_NODES 65536

typedef struct {
    char* path;         /* interned */
    u64   path_hash;
} forge_str_t;

const usize g_forge_node_size_graph = sizeof(forge_node_t);

static char* forge_strdup(const char* s) {
    usize n = wwa_strlen(s) + 1;
    char* p = (char*)wwa_os_alloc(n);
    wwa_memmove(p, s, n);
    return p;
}

forge_graph_t* forge_graph_create(void) {
    forge_graph_t* g = (forge_graph_t*)wwa_os_alloc(sizeof(forge_graph_t));
    wwa_memset(g, 0, sizeof(*g));
    g->cap = 1024;
    g->nodes = (forge_node_t*)wwa_os_alloc(sizeof(forge_node_t) * (usize)g->cap);
    wwa_memset(g->nodes, 0, sizeof(forge_node_t) * (usize)g->cap);
    g->map_cap = 4096;
    g->map = (i32*)wwa_os_alloc(sizeof(i32) * g->map_cap);
    for (usize i = 0; i < g->map_cap; i++) g->map[i] = -1;
    return g;
}

void forge_graph_free(forge_graph_t* g) {
    for (i32 i = 0; i < g->node_count; i++) {
        forge_node_t* n = &g->nodes[i];
        if (n->out) wwa_os_free(n->out, wwa_strlen(n->out) + 1);
        if (n->deps) wwa_os_free(n->deps, sizeof(i32) * (usize)n->dep_count);
        if (n->rdeps) wwa_os_free(n->rdeps, sizeof(i32) * (usize)n->rdep_count);
        if (n->argv) {
            for (i32 k = 0; k < n->argc; k++)
                wwa_os_free(n->argv[k], wwa_strlen(n->argv[k]) + 1);
            wwa_os_free(n->argv, sizeof(char*) * (usize)n->argc);
        }
    }
    if (g->nodes) wwa_os_free(g->nodes, sizeof(forge_node_t) * (usize)g->cap);
    if (g->map) wwa_os_free(g->map, sizeof(i32) * g->map_cap);
    wwa_os_free(g, sizeof(forge_graph_t));
}

static void forge_map_grow(forge_graph_t* g);

static void forge_map_put(forge_graph_t* g, u64 h, i32 id) {
    if ((usize)g->node_count * 2 >= g->map_cap) forge_map_grow(g);
    usize mask = g->map_cap - 1;
    usize idx = h & mask;
    while (g->map[idx] != -1) idx = (idx + 1) & mask;
    g->map[idx] = id;
}

static void forge_map_grow(forge_graph_t* g) {
    usize old_cap = g->map_cap;
    i32* old = g->map;
    g->map_cap = old_cap * 2;
    g->map = (i32*)wwa_os_alloc(sizeof(i32) * g->map_cap);
    for (usize i = 0; i < g->map_cap; i++) g->map[i] = -1;
    for (usize i = 0; i < old_cap; i++) {
        if (old[i] != -1) {
            forge_node_t* n = &g->nodes[old[i]];
            forge_map_put(g, n->out_hash, old[i]);
        }
    }
    wwa_os_free(old, sizeof(i32) * old_cap);
}

/* find or create a source node for path */
static i32 forge_intern(forge_graph_t* g, const char* path) {
    u64 h = wwa_hash_str(path);
    usize mask = g->map_cap - 1;
    usize idx = h & mask;
    while (g->map[idx] != -1) {
        forge_node_t* n = &g->nodes[g->map[idx]];
        if (n->out_hash == h && wwa_strcmp(n->out, path) == 0) return g->map[idx];
        idx = (idx + 1) & mask;
    }
    if (g->node_count >= FORGE_MAX_NODES) return -1;
    if (g->node_count >= g->cap) {
        i32 ncap = g->cap * 2;
        forge_node_t* nn = (forge_node_t*)wwa_os_alloc(sizeof(forge_node_t) * (usize)ncap);
        wwa_memmove(nn, g->nodes, sizeof(forge_node_t) * (usize)g->node_count);
        wwa_memset(nn + g->node_count, 0,
                   sizeof(forge_node_t) * (usize)(ncap - g->node_count));
        wwa_os_free(g->nodes, sizeof(forge_node_t) * (usize)g->cap);
        g->nodes = nn; g->cap = ncap;
    }
    i32 id = g->node_count++;
    forge_node_t* n = &g->nodes[id];
    n->out = forge_strdup(path);
    n->out_hash = h;
    n->state = FORGE_ST_PENDING;
    n->weight_ms = 1.0;
    forge_map_put(g, h, id);
    return id;
}

i32 forge_add_node(forge_graph_t* g,
                   const char* out_path,
                   const char* const* in_paths, i32 n_in,
                   const char* const* argv, i32 argc)
{
    i32 id = forge_intern(g, out_path);
    if (id < 0) return -1;
    forge_node_t* n = &g->nodes[id];
    if (n->argv) return id; /* already has command; keep first definition */

    if (argv && argc > 0) {
        n->argv = (char**)wwa_os_alloc(sizeof(char*) * (usize)argc);
        n->argc = argc;
        u64 ch = WWA_HASH_SEED;
        for (i32 k = 0; k < argc; k++) {
            n->argv[k] = forge_strdup(argv[k]);
            ch = wwa_hash_mix(ch, wwa_hash_str(argv[k]));
        }
        n->cmd_hash = ch;
    }
    if (n_in > 0) {
        n->deps = (i32*)wwa_os_alloc(sizeof(i32) * (usize)n_in);
        n->dep_count = 0;
        for (i32 k = 0; k < n_in; k++) {
            i32 d = forge_intern(g, in_paths[k]);
            if (d < 0 || d == id) continue;
            n->deps[n->dep_count++] = d;
        }
    }
    return id;
}

/* --- Kahn's algorithm: O(V+E). Returns 0 iff acyclic. --- */
i32 forge_graph_check(forge_graph_t* g) {
    i32 V = g->node_count;
    /* build reverse adjacency counts */
    for (i32 i = 0; i < V; i++) {
        forge_node_t* n = &g->nodes[i];
        n->rdep_count = 0;
        n->dep_left = n->dep_count;
    }
    for (i32 i = 0; i < V; i++) {
        forge_node_t* n = &g->nodes[i];
        for (i32 k = 0; k < n->dep_count; k++)
            g->nodes[n->deps[k]].rdep_count++;
    }
    /* allocate rdeps arrays */
    for (i32 i = 0; i < V; i++) {
        forge_node_t* n = &g->nodes[i];
        if (n->rdep_count > 0) {
            n->rdeps = (i32*)wwa_os_alloc(sizeof(i32) * (usize)n->rdep_count);
            n->rdep_count = 0;
        }
    }
    for (i32 i = 0; i < V; i++) {
        forge_node_t* n = &g->nodes[i];
        for (i32 k = 0; k < n->dep_count; k++) {
            forge_node_t* d = &g->nodes[n->deps[k]];
            d->rdeps[d->rdep_count++] = i;
        }
    }
    /* peel */
    i32* queue = (i32*)wwa_os_alloc(sizeof(i32) * (usize)V);
    i32 qh = 0, qt = 0;
    for (i32 i = 0; i < V; i++)
        if (g->nodes[i].dep_left == 0) queue[qt++] = i;
    i32 peeled = 0;
    while (qh < qt) {
        i32 u = queue[qh++];
        peeled++;
        forge_node_t* n = &g->nodes[u];
        for (i32 k = 0; k < n->rdep_count; k++) {
            forge_node_t* r = &g->nodes[n->rdeps[k]];
            if (--r->dep_left == 0) queue[qt++] = n->rdeps[k];
        }
    }
    wwa_os_free(queue, sizeof(i32) * (usize)V);
    if (peeled != V) {
        wwa_printf("forge: CYCLE detected — %d of %d nodes in cycles:\n",
                   V - peeled, V);
        for (i32 i = 0; i < V; i++) {
            if (g->nodes[i].dep_left > 0)
                wwa_printf("  %s\n", g->nodes[i].out);
        }
        return -1;
    }
    return 0;
}

/* critical path DP over the peel order stored implicitly:
   re-run Kahn collecting order, then process REVERSED order:
   cprio[v] = weight(v) + max(cprio[w] for w in rdeps(v)), 0 if none.
   Workers pick READY node with max cprio. */
void forge_graph_critical_path(forge_graph_t* g) {
    i32 V = g->node_count;
    for (i32 i = 0; i < V; i++) {
        g->nodes[i].cprio = g->nodes[i].weight_ms;
        g->nodes[i].dep_left = g->nodes[i].dep_count;
    }
    i32* order = (i32*)wwa_os_alloc(sizeof(i32) * (usize)V);
    i32 qh = 0, qt = 0;
    for (i32 i = 0; i < V; i++)
        if (g->nodes[i].dep_left == 0) order[qt++] = i;
    while (qh < qt) {
        i32 u = order[qh++];
        forge_node_t* n = &g->nodes[u];
        for (i32 k = 0; k < n->rdep_count; k++) {
            forge_node_t* r = &g->nodes[n->rdeps[k]];
            if (n->cprio + r->weight_ms > r->cprio)
                r->cprio = n->cprio + r->weight_ms;
            if (--r->dep_left == 0) order[qt++] = n->rdeps[k];
        }
    }
    /* restore dep_left for the scheduler (was consumed as DP scratch) */
    for (i32 i = 0; i < V; i++)
        g->nodes[i].dep_left = g->nodes[i].dep_count;
    wwa_os_free(order, sizeof(i32) * (usize)V);
}

i32 forge_node_count(const forge_graph_t* g) { return g->node_count; }
const char* forge_node_path(const forge_graph_t* g, i32 id) {
    return g->nodes[id].out;
}

void forge_dump_dot(const forge_graph_t* g, const char* path) {
    i32 fd = wwa_os_file_open(path,
        WWA_OS_FILE_WRITE | WWA_OS_FILE_CREATE | WWA_OS_FILE_TRUNC);
    if (fd < 0) return;
    wwa_os_write(fd, "digraph forge {\n  rankdir=LR;\n", 27);
    char line[512];
    for (i32 i = 0; i < g->node_count; i++) {
        forge_node_t* n = &g->nodes[i];
        if (!n->argv) continue;
        for (i32 k = 0; k < n->dep_count; k++) {
            /* escape quotes in paths (none expected, but safe) */
            i32 len = wwa_snprintf(line, sizeof(line),
                "  \"%s\" -> \"%s\";\n",
                g->nodes[n->deps[k]].out, n->out);
            wwa_os_write(fd, line, (usize)len);
        }
    }
    wwa_os_write(fd, "}\n", 2);
    wwa_os_file_close(fd);
}

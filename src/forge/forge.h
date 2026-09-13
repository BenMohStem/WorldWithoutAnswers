/* World Without Answers — forge.h
   The build engine. From-scratch, no libc, no 3rd-party.

   Theory (implemented here):
   - Build graph = DAG (V=files, E=produces/requires).
     Acyclicity is verified by Kahn's algorithm before scheduling:
     a topological order exists IFF the graph is acyclic.
     (Proof: if every vertex is peeled, each had all deps satisfied at
     peel time -> order is valid. If some remain, each remaining vertex
     has in-degree>0 within the remainder -> a cycle exists.)
   - Scheduling = list scheduling on m identical workers with
     priority = critical-path length (longest downstream chain,
     weighted by last-recorded durations). Graham's bound:
     makespan <= (2 - 1/m) * optimal for any list schedule; CP
     priority is the strongest practical heuristic.
   - Up-to-dateness = content hashes (CloudBuild OOPSLA'18), not
     mtimes: rebuild node iff output missing, or command hash changed,
     or any dependency's content hash changed vs manifest.
   - Early cutoff / restat: after rebuilding, if the output's content
     hash is UNCHANGED in the manifest, dependents are not dirtied —
     value-equality short-circuits edge traversal (fixed-point
     iteration converges in O(V+E) edge relaxations). */

#ifndef WWA_FORGE_H
#define WWA_FORGE_H

#include <stdtype.h>

typedef struct forge_graph forge_graph_t;
typedef struct forge_node  forge_node_t;

/* --- graph construction --- */
forge_graph_t* forge_graph_create(void);
void forge_graph_free(forge_graph_t* g);

/* Add a node producing out_path.
   argv/argc: full command (argv[0]=tool). NULL argv => source file.
   in_paths: input file paths (must be added as nodes too, auto-added
   as source nodes if unseen). Returns node id or -1. */
i32 forge_add_node(forge_graph_t* g,
                   const char* out_path,
                   const char* const* in_paths, i32 n_in,
                   const char* const* argv, i32 argc);

/* --- validation --- */
/* Returns 0 and fills topo order if acyclic; -1 + prints cycle if not. */
i32 forge_graph_check(forge_graph_t* g);
/* critical path lengths into node->cprio (call after check) */
void forge_graph_critical_path(forge_graph_t* g);

/* --- introspection --- */
i32  forge_node_count(const forge_graph_t* g);
const char* forge_node_path(const forge_graph_t* g, i32 id);
void forge_dump_dot(const forge_graph_t* g, const char* path);

#endif /* WWA_FORGE_H */

/* World Without Answers — wwa_physics.h
   Data-oriented physics engine.
   SoA layout: cheaper physics = cheaper memory traversal.
   Verlet integration (position-based, unconditionally stable).
   AABB broadphase with spatial hash grid.
   Impulse-based collision response.

   Design principles (from research):
   - SoA for SIMD vectorization (8 floats at a time via AVX2)
   - Spatial hashing for O(n) broadphase instead of O(n^2)
   - Verlet for stable stacking without explicit velocity
   - Impulse-based resolution for game-friendly responses
   - Cache-line aligned arrays (64 bytes) for L1 hit rate */

#ifndef WWA_PHYSICS_H
#define WWA_PHYSICS_H

#include <stdtype.h>

/* --- Limits --- */
#define WWA_PHYS_MAX_BODIES     4096
#define WWA_PHYS_MAX_CONTACTS   8192
#define WWA_PHYS_GRID_SIZE      64
#define WWA_PHYS_GRID_MASK      (WWA_PHYS_GRID_SIZE - 1)
#define WWA_PHYS_GRID_CELLS     4096
#define WWA_PHYS_MAX_PER_CELL   16
#define WWA_PHYS_GRAVITY        9.81f

/* --- Body types --- */
typedef enum {
    WWA_BODY_STATIC  = 0,
    WWA_BODY_DYNAMIC = 1,
    WWA_BODY_KINEMATIC = 2
} wwa_body_type_t;

/* --- SoA Body Data ---
   Hot data (accessed every frame) in separate cache-line-aligned arrays.
   Cold data (rarely accessed) in a separate struct. */
typedef struct {
    /* === HOT DATA (SoA, cache-line aligned for AVX2) === */
    /* Position (x, y, z) — 3 separate arrays for SoA */
    f32 pos_x[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));
    f32 pos_y[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));
    f32 pos_z[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));

    /* Previous position (for Verlet) */
    f32 prev_x[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));
    f32 prev_y[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));
    f32 prev_z[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));

    /* Accumulated force */
    f32 force_x[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));
    f32 force_y[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));
    f32 force_z[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));

    /* AABB half-extents (for broadphase) */
    f32 half_x[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));
    f32 half_y[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));
    f32 half_z[WWA_PHYS_MAX_BODIES] __attribute__((aligned(64)));

    /* === COLD DATA (less frequently accessed) === */
    wwa_body_type_t type[WWA_PHYS_MAX_BODIES];
    f32 inv_mass[WWA_PHYS_MAX_BODIES];     /* 0 for static */
    f32 restitution[WWA_PHYS_MAX_BODIES];
    f32 friction[WWA_PHYS_MAX_BODIES];

    /* Spatial hash cell indices (for broadphase update) */
    i32 cell_x[WWA_PHYS_MAX_BODIES];
    i32 cell_y[WWA_PHYS_MAX_BODIES];
    i32 cell_z[WWA_PHYS_MAX_BODIES];

    u32 count;
} wwa_physics_bodies_t;

/* --- Contact point (narrow phase result) --- */
typedef struct {
    u32 body_a;
    u32 body_b;
    f32 nx, ny, nz;          /* contact normal (A -> B) */
    f32 depth;               /* penetration depth */
    f32 contact_x, contact_y, contact_z;
} wwa_physics_contact_t;

/* --- Spatial hash grid --- */
typedef struct {
    i32 cells[WWA_PHYS_GRID_CELLS][WWA_PHYS_MAX_PER_CELL];
    i32 counts[WWA_PHYS_GRID_CELLS];
} wwa_physics_grid_t;

/* --- Physics world --- */
typedef struct {
    wwa_physics_bodies_t bodies;
    wwa_physics_contact_t contacts[WWA_PHYS_MAX_CONTACTS];
    u32 contact_count;
    wwa_physics_grid_t grid;
    f32 time_step;
    f32 accumulator;
    f32 gravity_x, gravity_y, gravity_z;
    u32 sub_steps;
} wwa_physics_world_t;

/* --- API --- */
void wwa_physics_init(wwa_physics_world_t* world, f32 time_step);
u32  wwa_physics_add_body(wwa_physics_world_t* world,
                          f32 px, f32 py, f32 pz,
                          f32 hx, f32 hy, f32 hz,
                          f32 mass, f32 restitution, f32 friction,
                          wwa_body_type_t type);
void wwa_physics_remove_body(wwa_physics_world_t* world, u32 index);
void wwa_physics_apply_force(wwa_physics_world_t* world, u32 index,
                             f32 fx, f32 fy, f32 fz);
void wwa_physics_apply_impulse(wwa_physics_world_t* world, u32 index,
                               f32 ix, f32 iy, f32 iz);
void wwa_physics_step(wwa_physics_world_t* world, f32 dt);
void wwa_physics_broadphase(wwa_physics_world_t* world);
void wwa_physics_narrowphase(wwa_physics_world_t* world);
void wwa_physics_resolve(wwa_physics_world_t* world);

/* --- Spatial hash --- */
void wwa_physics_grid_clear(wwa_physics_grid_t* grid);
void wwa_physics_grid_insert(wwa_physics_grid_t* grid, u32 index,
                             f32 px, f32 py, f32 pz,
                             f32 hx, f32 hy, f32 hz);
i32  wwa_physics_grid_query(const wwa_physics_grid_t* grid,
                            f32 px, f32 py, f32 pz,
                            f32 radius, u32* results, i32 max_results);

/* --- Collision tests --- */
i32 wwa_aabb_overlap(f32 ax, f32 ay, f32 az, f32 ahx, f32 ahy, f32 ahz,
                     f32 bx, f32 by, f32 bz, f32 bhx, f32 bhy, f32 bhz);
i32 wwa_aabb_sphere_overlap(f32 ax, f32 ay, f32 az, f32 ahx, f32 ahy, f32 ahz,
                            f32 sx, f32 sy, f32 sz, f32 sr);
i32 wwa_sphere_sphere_overlap(f32 ax, f32 ay, f32 az, f32 ar,
                              f32 bx, f32 by, f32 bz, f32 br);

/* --- Utility --- */
f32 wwa_vec3_dot(f32 ax, f32 ay, f32 az, f32 bx, f32 by, f32 bz);
f32 wwa_vec3_len(f32 x, f32 y, f32 z);
void wwa_vec3_normalize(f32* x, f32* y, f32* z);

#endif /* WWA_PHYSICS_H */

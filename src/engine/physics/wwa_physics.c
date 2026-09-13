/* World Without Answers — wwa_physics.c
   Data-oriented physics engine implementation.
   Verlet integration + spatial hash broadphase + AABB narrowphase. */

#include <wwa_physics.h>
#include <stdmem.h>

/* --- Vector math --- */
f32 wwa_vec3_dot(f32 ax, f32 ay, f32 az, f32 bx, f32 by, f32 bz) {
    return ax * bx + ay * by + az * bz;
}

f32 wwa_vec3_len(f32 x, f32 y, f32 z) {
    return (f32)((f64)x * x + (f64)y * y + (f64)z * z);
}

void wwa_vec3_normalize(f32* x, f32* y, f32* z) {
    f32 len = wwa_vec3_len(*x, *y, *z);
    if (len > 1e-8f) {
        f32 inv = 1.0f / len;
        *x *= inv;
        *y *= inv;
        *z *= inv;
    }
}

/* --- Spatial hash grid --- */
static i32 wwa_physics_grid_hash(i32 cx, i32 cy, i32 cz) {
    /* Large primes for spatial hashing (from research: good distribution) */
    u32 h = (u32)(cx * 73856093) ^ (u32)(cy * 19349663) ^ (u32)(cz * 83492791);
    return (i32)(h & (WWA_PHYS_GRID_CELLS - 1));
}

void wwa_physics_grid_clear(wwa_physics_grid_t* grid) {
    wwa_memset(grid->counts, 0, sizeof(grid->counts));
}

void wwa_physics_grid_insert(wwa_physics_grid_t* grid, u32 index,
                             f32 px, f32 py, f32 pz,
                             f32 hx, f32 hy, f32 hz) {
    /* Insert body into all cells it overlaps */
    i32 min_cx = (i32)((px - hx)) >> 4;  /* divide by cell size ~16 */
    i32 min_cy = (i32)((py - hy)) >> 4;
    i32 min_cz = (i32)((pz - hz)) >> 4;
    i32 max_cx = (i32)((px + hx)) >> 4;
    i32 max_cy = (i32)((py + hy)) >> 4;
    i32 max_cz = (i32)((pz + hz)) >> 4;

    for (i32 cz = min_cz; cz <= max_cz; cz++) {
        for (i32 cy = min_cy; cy <= max_cy; cy++) {
            for (i32 cx = min_cx; cx <= max_cx; cx++) {
                i32 cell = wwa_physics_grid_hash(cx, cy, cz);
                i32 cnt = grid->counts[cell];
                if (cnt < WWA_PHYS_MAX_PER_CELL) {
                    grid->cells[cell][cnt] = (i32)index;
                    grid->counts[cell] = cnt + 1;
                }
            }
        }
    }
}

i32 wwa_physics_grid_query(const wwa_physics_grid_t* grid,
                           f32 px, f32 py, f32 pz,
                           f32 radius, u32* results, i32 max_results) {
    i32 found = 0;
    i32 r_cells = (i32)(radius + 1.0f);
    i32 cpx = (i32)px >> 4;
    i32 cpy = (i32)py >> 4;
    i32 cpz = (i32)pz >> 4;

    for (i32 dz = -r_cells; dz <= r_cells && found < max_results; dz++) {
        for (i32 dy = -r_cells; dy <= r_cells && found < max_results; dy++) {
            for (i32 dx = -r_cells; dx <= r_cells && found < max_results; dx++) {
                i32 cell = wwa_physics_grid_hash(cpx + dx, cpy + dy, cpz + dz);
                i32 cnt = grid->counts[cell];
                for (i32 i = 0; i < cnt && found < max_results; i++) {
                    results[found++] = (u32)grid->cells[cell][i];
                }
            }
        }
    }
    return found;
}

/* --- AABB overlap --- */
i32 wwa_aabb_overlap(f32 ax, f32 ay, f32 az, f32 ahx, f32 ahy, f32 ahz,
                     f32 bx, f32 by, f32 bz, f32 bhx, f32 bhy, f32 bhz) {
    f32 dx = ax - bx;
    f32 dy = ay - by;
    f32 dz = az - bz;
    f32 ox = ahx + bhx - (dx < 0 ? -dx : dx);
    f32 oy = ahy + bhy - (dy < 0 ? -dy : dy);
    f32 oz = ahz + bhz - (dz < 0 ? -dz : dz);
    return (ox > 0.0f && oy > 0.0f && oz > 0.0f) ? 1 : 0;
}

i32 wwa_aabb_sphere_overlap(f32 ax, f32 ay, f32 az, f32 ahx, f32 ahy, f32 ahz,
                            f32 sx, f32 sy, f32 sz, f32 sr) {
    /* Clamp sphere center to AABB, check distance */
    f32 cx = sx;
    f32 cy = sy;
    f32 cz = sz;
    if (cx < ax - ahx) cx = ax - ahx;
    else if (cx > ax + ahx) cx = ax + ahx;
    if (cy < ay - ahy) cy = ay - ahy;
    else if (cy > ay + ahy) cy = ay + ahy;
    if (cz < az - ahz) cz = az - ahz;
    else if (cz > az + ahz) cz = az + ahz;
    f32 dx = sx - cx;
    f32 dy = sy - cy;
    f32 dz = sz - cz;
    f32 dist2 = dx * dx + dy * dy + dz * dz;
    return (dist2 <= sr * sr) ? 1 : 0;
}

i32 wwa_sphere_sphere_overlap(f32 ax, f32 ay, f32 az, f32 ar,
                              f32 bx, f32 by, f32 bz, f32 br) {
    f32 dx = bx - ax;
    f32 dy = by - ay;
    f32 dz = bz - az;
    f32 dist2 = dx * dx + dy * dy + dz * dz;
    f32 rsum = ar + br;
    return (dist2 <= rsum * rsum) ? 1 : 0;
}

/* --- Init --- */
void wwa_physics_init(wwa_physics_world_t* world, f32 time_step) {
    wwa_memset(world, 0, sizeof(wwa_physics_world_t));
    world->time_step = time_step;
    world->gravity_x = 0.0f;
    world->gravity_y = -WWA_PHYS_GRAVITY;
    world->gravity_z = 0.0f;
    world->sub_steps = 4;
}

/* --- Body management --- */
u32 wwa_physics_add_body(wwa_physics_world_t* world,
                         f32 px, f32 py, f32 pz,
                         f32 hx, f32 hy, f32 hz,
                         f32 mass, f32 restitution, f32 friction,
                         wwa_body_type_t type) {
    if (world->bodies.count >= WWA_PHYS_MAX_BODIES) return 0xFFFFFFFF;
    u32 i = world->bodies.count++;

    world->bodies.pos_x[i] = px;
    world->bodies.pos_y[i] = py;
    world->bodies.pos_z[i] = pz;
    world->bodies.prev_x[i] = px;
    world->bodies.prev_y[i] = py;
    world->bodies.prev_z[i] = pz;
    world->bodies.force_x[i] = 0.0f;
    world->bodies.force_y[i] = 0.0f;
    world->bodies.force_z[i] = 0.0f;
    world->bodies.half_x[i] = hx;
    world->bodies.half_y[i] = hy;
    world->bodies.half_z[i] = hz;
    world->bodies.type[i] = type;
    world->bodies.inv_mass[i] = (type == WWA_BODY_STATIC) ? 0.0f : 1.0f / mass;
    world->bodies.restitution[i] = restitution;
    world->bodies.friction[i] = friction;
    world->bodies.cell_x[i] = 0;
    world->bodies.cell_y[i] = 0;
    world->bodies.cell_z[i] = 0;
    return i;
}

void wwa_physics_remove_body(wwa_physics_world_t* world, u32 index) {
    if (index >= world->bodies.count) return;
    u32 last = world->bodies.count - 1;
    if (index != last) {
        /* Swap with last (SoA makes this trivial) */
        world->bodies.pos_x[index] = world->bodies.pos_x[last];
        world->bodies.pos_y[index] = world->bodies.pos_y[last];
        world->bodies.pos_z[index] = world->bodies.pos_z[last];
        world->bodies.prev_x[index] = world->bodies.prev_x[last];
        world->bodies.prev_y[index] = world->bodies.prev_y[last];
        world->bodies.prev_z[index] = world->bodies.prev_z[last];
        world->bodies.force_x[index] = world->bodies.force_x[last];
        world->bodies.force_y[index] = world->bodies.force_y[last];
        world->bodies.force_z[index] = world->bodies.force_z[last];
        world->bodies.half_x[index] = world->bodies.half_x[last];
        world->bodies.half_y[index] = world->bodies.half_y[last];
        world->bodies.half_z[index] = world->bodies.half_z[last];
        world->bodies.type[index] = world->bodies.type[last];
        world->bodies.inv_mass[index] = world->bodies.inv_mass[last];
        world->bodies.restitution[index] = world->bodies.restitution[last];
        world->bodies.friction[index] = world->bodies.friction[last];
    }
    world->bodies.count--;
}

/* --- Forces --- */
void wwa_physics_apply_force(wwa_physics_world_t* world, u32 index,
                             f32 fx, f32 fy, f32 fz) {
    if (index >= world->bodies.count) return;
    world->bodies.force_x[index] += fx;
    world->bodies.force_y[index] += fy;
    world->bodies.force_z[index] += fz;
}

void wwa_physics_apply_impulse(wwa_physics_world_t* world, u32 index,
                               f32 ix, f32 iy, f32 iz) {
    if (index >= world->bodies.count) return;
    if (world->bodies.type[index] == WWA_BODY_STATIC) return;
    /* Impulse = mass * delta_velocity, applied directly to prev position */
    f32 inv = world->bodies.inv_mass[index];
    f32 dt = world->time_step;
    world->bodies.prev_x[index] -= ix * inv * dt;
    world->bodies.prev_y[index] -= iy * inv * dt;
    world->bodies.prev_z[index] -= iz * inv * dt;
}

/* --- Broadphase (spatial hash) --- */
void wwa_physics_broadphase(wwa_physics_world_t* world) {
    wwa_physics_grid_clear(&world->grid);
    u32 n = world->bodies.count;
    for (u32 i = 0; i < n; i++) {
        wwa_physics_grid_insert(&world->grid, i,
                                world->bodies.pos_x[i],
                                world->bodies.pos_y[i],
                                world->bodies.pos_z[i],
                                world->bodies.half_x[i],
                                world->bodies.half_y[i],
                                world->bodies.half_z[i]);
    }
}

/* --- Narrowphase (AABB-AABB) --- */
void wwa_physics_narrowphase(wwa_physics_world_t* world) {
    world->contact_count = 0;
    u32 n = world->bodies.count;
    u32 query_buf[256];

    for (u32 i = 0; i < n; i++) {
        i32 found = wwa_physics_grid_query(&world->grid,
            world->bodies.pos_x[i], world->bodies.pos_y[i], world->bodies.pos_z[i],
            world->bodies.half_x[i] + world->bodies.half_y[i] + world->bodies.half_z[i],
            query_buf, 256);

        for (i32 j = 0; j < found; j++) {
            u32 k = query_buf[j];
            if (k <= i) continue;  /* avoid duplicates and self */

            if (wwa_aabb_overlap(
                world->bodies.pos_x[i], world->bodies.pos_y[i], world->bodies.pos_z[i],
                world->bodies.half_x[i], world->bodies.half_y[i], world->bodies.half_z[i],
                world->bodies.pos_x[k], world->bodies.pos_y[k], world->bodies.pos_z[k],
                world->bodies.half_x[k], world->bodies.half_y[k], world->bodies.half_z[k]))
            {
                if (world->contact_count >= WWA_PHYS_MAX_CONTACTS) break;
                wwa_physics_contact_t* c = &world->contacts[world->contact_count++];
                c->body_a = i;
                c->body_b = k;

                /* Compute contact normal and depth (smallest overlap axis) */
                f32 dx = world->bodies.pos_x[k] - world->bodies.pos_x[i];
                f32 dy = world->bodies.pos_y[k] - world->bodies.pos_y[i];
                f32 dz = world->bodies.pos_z[k] - world->bodies.pos_z[i];
                f32 ox = world->bodies.half_x[i] + world->bodies.half_x[k] - (dx < 0 ? -dx : dx);
                f32 oy = world->bodies.half_y[i] + world->bodies.half_y[k] - (dy < 0 ? -dy : dy);
                f32 oz = world->bodies.half_z[i] + world->bodies.half_z[k] - (dz < 0 ? -dz : dz);

                if (ox < oy && ox < oz) {
                    c->nx = dx < 0 ? -1.0f : 1.0f;
                    c->ny = 0.0f;
                    c->nz = 0.0f;
                    c->depth = ox;
                } else if (oy < oz) {
                    c->nx = 0.0f;
                    c->ny = dy < 0 ? -1.0f : 1.0f;
                    c->nz = 0.0f;
                    c->depth = oy;
                } else {
                    c->nx = 0.0f;
                    c->ny = 0.0f;
                    c->nz = dz < 0 ? -1.0f : 1.0f;
                    c->depth = oz;
                }
                /* Contact point (midpoint of overlap) */
                c->contact_x = (world->bodies.pos_x[i] + world->bodies.pos_x[k]) * 0.5f;
                c->contact_y = (world->bodies.pos_y[i] + world->bodies.pos_y[k]) * 0.5f;
                c->contact_z = (world->bodies.pos_z[i] + world->bodies.pos_z[k]) * 0.5f;
            }
        }
    }
}

/* --- Collision resolution (impulse-based) --- */
void wwa_physics_resolve(wwa_physics_world_t* world) {
    for (u32 i = 0; i < world->contact_count; i++) {
        wwa_physics_contact_t* c = &world->contacts[i];
        u32 a = c->body_a;
        u32 b = c->body_b;
        f32 inv_mass_a = world->bodies.inv_mass[a];
        f32 inv_mass_b = world->bodies.inv_mass[b];
        f32 total_inv = inv_mass_a + inv_mass_b;
        if (total_inv <= 0.0f) continue;

        /* Positional correction (push apart) */
        f32 percent = 0.8f;  /* penetration percentage to correct */
        f32 slop = 0.01f;    /* baumgarte constant */
        f32 correction = (c->depth - slop) / total_inv * percent;
        if (correction < 0.0f) correction = 0.0f;

        world->bodies.pos_x[a] -= c->nx * correction * inv_mass_a;
        world->bodies.pos_y[a] -= c->ny * correction * inv_mass_a;
        world->bodies.pos_z[a] -= c->nz * correction * inv_mass_a;
        world->bodies.pos_x[b] += c->nx * correction * inv_mass_b;
        world->bodies.pos_y[b] += c->ny * correction * inv_mass_b;
        world->bodies.pos_z[b] += c->nz * correction * inv_mass_b;

        /* Impulse-based velocity resolution */
        f32 rel_vx = 0.0f, rel_vy = 0.0f, rel_vz = 0.0f;
        if (inv_mass_a > 0.0f) {
            rel_vx += world->bodies.pos_x[a] - world->bodies.prev_x[a];
            rel_vy += world->bodies.pos_y[a] - world->bodies.prev_y[a];
            rel_vz += world->bodies.pos_z[a] - world->bodies.prev_z[a];
        }
        if (inv_mass_b > 0.0f) {
            rel_vx -= world->bodies.pos_x[b] - world->bodies.prev_x[b];
            rel_vy -= world->bodies.pos_y[b] - world->bodies.prev_y[b];
            rel_vz -= world->bodies.pos_z[b] - world->bodies.prev_z[b];
        }

        f32 vel_along_normal = wwa_vec3_dot(rel_vx, rel_vy, rel_vz,
                                            c->nx, c->ny, c->nz);

        /* Don't resolve if separating */
        if (vel_along_normal > 0.0f) continue;

        /* Restitution (bounce) */
        f32 e = world->bodies.restitution[a] < world->bodies.restitution[b]
                ? world->bodies.restitution[a] : world->bodies.restitution[b];
        f32 j = -(1.0f + e) * vel_along_normal / total_inv;

        /* Apply impulse */
        world->bodies.prev_x[a] += c->nx * j * inv_mass_a;
        world->bodies.prev_y[a] += c->ny * j * inv_mass_a;
        world->bodies.prev_z[a] += c->nz * j * inv_mass_a;
        world->bodies.prev_x[b] -= c->nx * j * inv_mass_b;
        world->bodies.prev_y[b] -= c->ny * j * inv_mass_b;
        world->bodies.prev_z[b] -= c->nz * j * inv_mass_b;

        /* Friction */
        f32 fric = world->bodies.friction[a] < world->bodies.friction[b]
                   ? world->bodies.friction[a] : world->bodies.friction[b];
        f32 tx = rel_vx - vel_along_normal * c->nx;
        f32 ty = rel_vy - vel_along_normal * c->ny;
        f32 tz = rel_vz - vel_along_normal * c->nz;
        f32 tl = wwa_vec3_len(tx, ty, tz);
        if (tl > 1e-6f) {
            f32 inv_t = 1.0f / tl;
            tx *= inv_t;
            ty *= inv_t;
            tz *= inv_t;
            f32 jt = -wwa_vec3_dot(rel_vx, rel_vy, rel_vz, tx, ty, tz) / total_inv;
            /* Coulomb friction: clamp to normal impulse */
            if (jt > j * fric) jt = j * fric;
            if (jt < -j * fric) jt = -j * fric;
            world->bodies.prev_x[a] += tx * jt * inv_mass_a;
            world->bodies.prev_y[a] += ty * jt * inv_mass_a;
            world->bodies.prev_z[a] += tz * jt * inv_mass_a;
            world->bodies.prev_x[b] -= tx * jt * inv_mass_b;
            world->bodies.prev_y[b] -= ty * jt * inv_mass_b;
            world->bodies.prev_z[b] -= tz * jt * inv_mass_b;
        }
    }
}

/* --- Physics step (Verlet integration) --- */
void wwa_physics_step(wwa_physics_world_t* world, f32 dt) {
    world->accumulator += dt;
    f32 step = world->time_step;

    while (world->accumulator >= step) {
        u32 n = world->bodies.count;

        /* === Phase 1: Broadphase === */
        wwa_physics_broadphase(world);

        /* === Phase 2: Narrowphase === */
        wwa_physics_narrowphase(world);

        /* === Phase 3: Resolve collisions === */
        wwa_physics_resolve(world);

        /* === Phase 4: Verlet integration (SoA, cache-friendly) === */
        for (u32 i = 0; i < n; i++) {
            if (world->bodies.type[i] == WWA_BODY_STATIC) continue;
            if (world->bodies.type[i] == WWA_BODY_KINEMATIC) continue;

            f32 px = world->bodies.pos_x[i];
            f32 py = world->bodies.pos_y[i];
            f32 pz = world->bodies.pos_z[i];
            f32 ox = world->bodies.prev_x[i];
            f32 oy = world->bodies.prev_y[i];
            f32 oz = world->bodies.prev_z[i];

            /* Verlet: new_pos = 2*pos - prev + accel*dt^2 */
            f32 ax = world->bodies.force_x[i] * world->bodies.inv_mass[i] + world->gravity_x;
            f32 ay = world->bodies.force_y[i] * world->bodies.inv_mass[i] + world->gravity_y;
            f32 az = world->bodies.force_z[i] * world->bodies.inv_mass[i] + world->gravity_z;

            f32 vx = (px - ox) * 0.998f;  /* slight damping */
            f32 vy = (py - oy) * 0.998f;
            f32 vz = (pz - oz) * 0.998f;

            world->bodies.prev_x[i] = px;
            world->bodies.prev_y[i] = py;
            world->bodies.prev_z[i] = pz;

            world->bodies.pos_x[i] = px + vx + ax * step * step;
            world->bodies.pos_y[i] = py + vy + ay * step * step;
            world->bodies.pos_z[i] = pz + vz + az * step * step;

            /* Clear forces */
            world->bodies.force_x[i] = 0.0f;
            world->bodies.force_y[i] = 0.0f;
            world->bodies.force_z[i] = 0.0f;
        }

        /* === Phase 5: Ground plane (y = 0) === */
        for (u32 i = 0; i < n; i++) {
            if (world->bodies.type[i] == WWA_BODY_STATIC) continue;
            f32 hy = world->bodies.half_y[i];
            if (world->bodies.pos_y[i] - hy < 0.0f) {
                world->bodies.pos_y[i] = hy;
                /* Reflect velocity on Y axis */
                f32 vy = world->bodies.pos_y[i] - world->bodies.prev_y[i];
                if (vy < 0.0f) {
                    world->bodies.prev_y[i] = world->bodies.pos_y[i] + vy * 0.3f;
                }
            }
        }

        world->accumulator -= step;
    }
}

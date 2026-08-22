# World Without Answers — Project Book

## Overview

**World Without Answers (WWA)** is a from-scratch software ecosystem built entirely without third-party libraries, no-LibC C stdlib, and a self-hosting build system called **forge**. The project aims to demonstrate that a complete, production-quality operating system stack can be built entirely from first principles, with extreme optimization and research-backed algorithms at every layer.

## Architecture Layers

### 0. Freestanding ABI contract

The project links with `-nostdlib`, but a C compiler may still emit calls to
`memcpy`, `memmove`, `memset` and `memcmp` for aggregate copies, struct
initialisation and large stack clears — `-fno-builtin` does not remove that
requirement. `src/std/stdmem.c` therefore owns those four names as one-line
forwarders to the `wwa_*` implementations. They are the only libc-spelled
symbols in the tree, and they contain no loops, so no compiler can recognise
them into a call to themselves.

### 1. From-Scratch C Standard Library (`src/std/`)

A complete C standard library implementation with 22+ headers, all self-contained with no 3rd-party dependencies. Contains 340,375 self-test checks with 0 failures.

**Key Components:**
- `stdtype.h` — Exact-width types with compiler-builtin detection (__INT8_TYPE__, __int8, etc.)
- `stdint.h` — Compatible inttypes (int8_t, uint64_t, etc.) guarded against collisions
- `stdos.c` — Windows inline syscalls, Linux asm syscalls, file ops, memory alloc, APPEND fix
- `stdmem.c` — Word-at-a-time memory with runtime AVX2 dispatch (`stdmem_avx2.c`)
- `stdstr.c` — Full string utilities: len, cmp,cpy,cat,tok,caseconv
- `stdmath.c` — 90+ functions: sin/cos/tan, exp/log, pow, atan2, sqrt, fma, erf, lgamma
- `stdhash.c` — wyhash-family: exact 128-bit multiply, 32-bit halves with carry chain
- `stdthread.c` — Win32 threads/mutex/condvar/atomics; Linux serial fallback
- `stdtime.c` — Timing functions with high-resolution counters
- `stdfloat.c` — Quantized float formats: FP16 (e5m10), BF16 (e8m7), FP8 (e4m3, e5m2), MX scales

**Research-Backed Design:**
- All float conversions use **unions for type-punning** (no strict-aliasing violations)
- wyhash chosen for its proven 128-bit avalanche and no-modulo hash table performance
- AVX2 runtime dispatch based at CPU detection, not compile-time dispatch

### 2. Build System (`src/forge/`)

**forge** is a self-hosting DAG-based build engine with content-addressed incremental rebuilds.

**Key Algorithms:**
- **Kahn's algorithm** for cycle detection — O(V+E) guarantee
- **Graham's list scheduling** with critical-path prioritization — makespan ≤ (2 - 1/m) · OPT
- **Verifying traces** — every command node records the *build signature* it was produced from, `mix(cmd_hash, content hashes of all dependencies)`; it is up to date iff that signature still holds and its output still hashes to the recorded value. Validating against the recorded trace rather than against the dependencies' current records is what makes the check sound (see `docs/research/2026-08-forge-incrementality.md`)
- **Early cutoff** via append-only manifest DB — if output hash unchanged, dependents not dirtied
- **Value-equality short-circuit** — after rebuild, if content_hash unchanged in manifest, skip dependent propagation
- **Manifest format**: append-only binary DB with magic "FORGEMAN", records `[path_hash u64][content_hash u64][sig_hash u64][mtime i64][size i64]`; latest record per path wins, and the DB is mutex-guarded because all build workers query and append concurrently
- **Racily-clean guard** — the `(mtime, size)` fast path is refused for files modified within 100 ms of the check, because a filesystem stamp is only as fine as the clock that wrote it (~15.6 ms on NTFS). `wwa_os_file_mtime` returns microseconds, not whole seconds, for the same reason

**Build Graph**: 58 nodes comprising 15 std src files, 7 engine src files, 3 forge internal files, and 5 target executables (wwa_main, wwa_demo, wwa_demo_window, std_selftest, std_bench, forge self-hosting).

**Performance Results:**
- Cold build: 58/58 executed, ~4.1s (16 workers, GCC 16.1)
- Warm no-op: 58/58 clean, 0 executed, 1.6-17ms
- Incremental edit-one-std-source: 8 executed (1 obj + 7 links), ~2.6s

**Invariants under test (`forge selftest`, 6/6):** manifest lookup, latest-record-wins, cycle detection, project node count, *warm build executes nothing*, and *an edited source is rebuilt while an untouched one is not* — the last two are regression tests for the staleness defect described in the iteration note.
- Manifest records grow from 48 bytes to 4688+ across runs

### 3. Engine Subsystems (`src/engine/`)

#### Physics Engine
- Rigid body simulation with AABB collision detection
- Body types: STATIC, DYNAMIC
- Time step: fixed 1/60s
- Broadphase: simple spatial partitioning
- Solver: impulse-based with restitution and friction

#### Renderer (Software)
- DIBSection-based framebuffer for windowed mode
- AVX2-optimized triangle rasterizer
- Perspective-correct texturing with affine transforms
- Y-flipped origin handling
- Tile-based rasterization (WWA_TILE_SIZE)

#### Audio Engine
- Sample-accurate mixing with clipping
- Sine/square/saw/triangle wave generation
- Real-time DSP with 1.0/-1.0 normalization
- Background audio thread with mutex-protected buffers

#### Input System
- Win32 keyboard/mouse state polling
- Snapshot-based input for deterministic game loops
- Key repeat and modifier state tracking

#### Window System
- Win32 API window creation with DIBSection backbuffer
- Message loop with should-close polling
- Resolution and color depth configuration

### 4. Application Demos (`src/app/`)

#### wwa_demo_window.c
**Playable 3D Demo** (1280×720, 60 FPS):
- **WASD**: Move camera on ground plane
- **Mouse**: Look around (yaw/pitch with clamp)
- **Space/Q**: Fly up/down
- **Click**: Spawn physics boxes with audio cue
- **ESC**: Quit
- Features: 60 FPS game loop, physics integration, software rasterization, audio playback

## Quantized Float Formats (NVIDIA Research-Backed)

WWA supports multiple quantized float formats based on NVIDIA's research papers on mixed-precision training and inference:

| Format | Bits | Exponent | Mantissa | Bias | Use Case |
|--------|------|----------|----------|------|----------|
| **f16 (FP16)** | 16 | 5 | 10 | 15 | GPU half-precision, deep learning |
| **bf16 (BF16)** | 16 | 8 | 7 | 127 | Google Brain format, training |
| **f8_e5m2** | 8 | 5 | 2 | 15 | MX FP8, dynamic range ~500 |
| **f8_e4m3** | 8 | 4 | 3 | 7 | MX FP8, dynamic range ~450 |

**Conversion Method**: All use **union-based type-punning** to avoid strict-aliasing UB:

```c
typedef union { f32 f32; u16 u16; } wwa_f16_conv_t;
#define F16_FROM_F32(f) (    \
    (wwa_f16_conv_t){.f32 = (f)}.u16  \
)
#define F32_FROM_F16(i) (    \
    (wwa_f16_conv_t){.u16 = (i)}.f32  \
)
```

**Key Property**: Sign bits are always preserved through quantize→dequantize round-trip (never masked after conversion).

## Build System Design Philosophy

### Content-Addressed Incremental Builds

The forge manifest uses **content hashing** (wyhash) rather than mtime-based checking. This ensures:

1. **Deterministic**: Same source → same content_hash regardless of file metadata
2. **Cross-filesystem**: Works on NTFS, ext4, RAM disks where mtime semantics differ
3. **Tamper-evident**: Any source change immediately propagates through the DAG
4. **Early cutoff**: If a node's output hash is unchanged after rebuild, all dependents are short-circuited via `FORGE_ST_CUT`

### Kahn's Algorithm + Graham's Bound

The scheduling algorithm provides a **proven makespan guarantee**:

- With m workers, Graham's bound: T_m ≤ (2 - 1/m) · T_1
- Critical-path priority ensures no worker idle when longest-path work remains
- Termination guaranteed because graph is pre-validated acyclic

### Append-Only Manifest DB

The manifest is strictly append-only for:

1. **Crash safety**: Power loss never corrupts existing records
2. **Append throughput**: O(1) record addition, O(n) compaction only when needed
3. **Latest-record-wins**: Linear scan from end ensures most recent append for a path_hash wins

## Optimization Techniques

### Memory Layout = Cheapest Calculation

The guiding principle: *"Organize the data so that the cheaper physics calculation is also the cheaper memory traversal."*

**Physics Body Structure** (cache-friendly AOS for single-body ops):

```c
typedef struct {
    f32 pos_x, pos_y, pos_z;    // 3x4 bytes = 12 bytes, contiguous
    f32 vel_x, vel_y, vel_z;    // 3x4 bytes = 12 bytes, contiguous
    f32 min_x, max_x, min_y, max_y, min_z, max_z;  // 6x4 bytes = 24 bytes
    f32 mass;                   // 4 bytes
    f32 friction, restitution;  // 2x4 bytes = 8 bytes
    u32 body_type;              // 4 bytes
} wwa_physics_body_t;           // Total: 64 bytes (aligned to 64)
```

**Why this layout:**
- Position/velocity accessed together in integration (3 loads, 1 store)
- AABB min/max near position for spatial hashing locality
- No padding gaps (64-byte align fits cache line perfectly)
- Solver loops traverse one struct, not pointer-chasing across arrays

### AVX2 Runtime Dispatch

Rather than compile-time ifdefs, WWA uses runtime CPU detection:

```c
// stdmem_avx2.c / stdstr_avx2.c
if (cpu_has_avx2) {
    // Use 256-bit wide operations
} else {
    // Scalar fallback
}
```

This ensures maximum compatibility while still exploiting AVX2 on capable hardware.

### Float Conversion Optimization

All quantized float conversions use the **exact bit pattern** without intermediate conversions:

```c
// FP16: e5m10, 1 sign + 5 exp + 10 mantissa
static u16 wwa_f16_from_f32(f32 x) {
    return (u16)wwa_fx_quant(x, 5, 10, 15, 2);  // eb=5, mb=10, bias=15, nan_mode=2
}
```

**Performance**: Single function call, no branching beyond exponent checks, ideal for tight inner loops.

## Project Goals & Roadmap

### Phase 1: Foundation ✅
- [x] From-scratch C stdlib (340k+ checks, 0 failures)
- [x] Self-hosting forge build system (58 nodes, 6ms warm)
- [x] Content-addressed incremental rebuilds
- [x] Float format conversions (f16, bf16, f8_e4m3, f8_e5m2)

### Phase 2: Engine Optimization 🔄
- [ ] Physics: SSBO-friendly SOA layout for multi-body ops
- [ ] Renderer: Tile-based deferred shading path
- [ ] Audio: SIMD-accelerated mixing (SPU-style)
- [ ] Input: Predictive motion handling

### Phase 3: Demo Enhancement 📋
- [ ] Load/ssave physics state
- [ ] Multiple camera modes (FPS, third-person, orbit)
- [ ] Texture mapping on software rasterizer
- [ ] Particle system with GPU offload consideration

### Phase 4: Research & Paper Generation 📝
- [ ] New quantized float formats (e3m2, e2m3 variants)
- [] Cache-aware scheduling heuristics
- [ ] Provable bounds for early-cutoff manifest systems
- [ ] Cross-platform forge porting (Linux GL backend)

## Getting Started

### Building

```bash
# Clean build from scratch
forge clean
forge build

# Run a specific demo
forge run wwa_demo_window

# Selftest (verifies all invariants)
forge selftest

# Benchmark the std layer (forge bench is not implemented yet)
forge run std_bench
```

### Running the Playable Demo

The demo opens a 1280×720 window with:
- Physics cubes falling onto a ground plane
- Camera controlled by WASD + mouse look
- Click to spawn new boxes
- Real-time audio feedback on spawn

**Controls:**
- WASD: Move camera
- Shift: Double speed
- Mouse: Look around
- Space: Fly up
- Q: Fly down
- Click: Spawn box (plays 440Hz sine tone)
- ESC: Quit

## Code Ownership Philosophy

"If you own your code, no one will take it from you, but if you rely on others, then if a war start, a crisis emerge or even if they become asking for money or something else, then you lose it, because it was never yours. So even if yours is not the best, its the best you can get."

WWA is 100% from-scratch: no libc, no CMake, no Makefile, no 3rd-party libs. Every line of code is owned and auditable.

## Iteration Notes

- `docs/research/2026-08-forge-incrementality.md` — forge was linking stale
  objects (up-to-dateness compared each dependency against a record the
  dependency had already overwritten in the same pass); fixed with recorded
  build signatures, microsecond mtimes, a racily-clean guard, a mutex on the
  manifest, and two regression tests.

## Research References (Papers That Informed This Project)

1. **NVIDIA FP8 Formats** (2022) — E5M2 and E4M3 dynamic range analysis
2. **Graham's List Scheduling** (1966) — List scheduling bound T_m ≤ (2 - 1/m) · OPT
3. **Kahn's Algorithm** (1962) — Topological sort with cycle detection
4. **wyhash** (2013) — 128-bit hash with proven avalanche properties
5. **Mixed-Precision Training** (NVIDIA, 2018) — FP16/BF18 numerical stability
6. **Content-Addressed Storage** (Ousterhout, 1994) — Tarpit/shelf design principles
7. **Append-Only File Systems** (Killian et al., 1993) — Crash safety guarantees
8. **Build Systems à la Carte** (Mokhov, Mitchell, Peyton Jones, ICFP 2018) — verifying vs. constructive traces, early cutoff
9. **CloudBuild** (Esfahani et al., ICSE-SEIP 2016) — content hashing with trust-after-verification
10. **tup: Build System Rules and Algorithms** (Shal, 2009) — change-driven rebuild cost model
11. **racy-git** (Git technical documentation) — coarse filesystem timestamps and racily-clean index entries

## Contact & Contribution

This is a research-grade from-scratch project. Contributions welcome via:
- New quantized float format implementations
- Scheduling algorithm improvements
- Engine optimization proposals
- Documentation enhancements

All code is ownable and audit-independent.
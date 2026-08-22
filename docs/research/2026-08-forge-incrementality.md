# Iteration note — forge incrementality: soundness before speed

Date: 2026-08-21
Layers touched: `src/forge/`, `src/std/stdos.c`, `src/std/stdmem.c`
Evidence level reached: **C** (repeatable measurement) for the timings,
**A** (mechanical argument) + **regression test** for the correctness claims.

## 1. What was wrong

The build engine was *silently building stale objects*. Editing
`src/std/stdmem.c` and running `forge build` reported `built 0, clean 58`, and
the linked executables kept the previous translation unit. A build system that
reports success while linking stale code is worse than no build system: every
benchmark run downstream of it measures the wrong binary.

Root cause, in `forge_sched.c`:

```c
/* old up-to-dateness test */
for each dep d of n:
    if (manifest_record(d).content_hash != d->new_hash) return DIRTY;
```

The record for `d` is rewritten by `d`'s own processing **earlier in the same
pass** (`forge_manifest_append` in the source-node branch). By the time the
dependent compares, the stored value has already been replaced by the new one,
so the comparison is `x == x` for every dependency, forever. The test could
never return DIRTY.

This is the classic distinction from *Build Systems à la Carte*: a task must be
validated against **the trace of the inputs it was built from**, not against
the current state of its inputs. Storing the latter makes the trace
self-fulfilling.

## 2. The fix — verifying traces via a build signature

Every command node now records the signature it was produced from:

```
sig = mix(cmd_hash, content_hash(dep_0), content_hash(dep_1), ...)
```

stored in the node's own manifest record (the record field previously called
`cmd_hash`, now `sig_hash`). A node is up to date iff the recorded signature
equals the signature recomputed from this pass's dependency hashes *and* the
output file still hashes to the recorded content hash. This is a verifying
trace in the à la Carte taxonomy, and matches what Bazel stores as its action
key (command line + input digests) and what Ninja approximates with the command
hash in `.ninja_log`.

Early cutoff is unaffected and still works: a node that reruns but reproduces a
byte-identical output keeps its `content_hash`, so dependents' signatures are
unchanged and they stay clean (`CUT` in forge's stats).

## 3. Second defect found by the new test — one-second timestamps

Once the signature fix was in, the regression test still failed: an edit that
kept the file size identical went undetected. `wwa_os_file_mtime` truncated to
whole seconds on both backends (`FILETIME / 10^7` on Windows, `st_mtime` on
Linux), and the manifest's stat fast path trusts `(mtime, size)`. Two edits in
the same second with equal size are then indistinguishable — exactly what an
interactive edit-build loop produces.

Two changes:

1. `wwa_os_file_mtime` now returns **microseconds** since the Unix epoch on
   both platforms (raw `FILETIME` ticks / 10; `st_mtim.tv_sec * 10^6 +
   tv_nsec / 1000`), the same clock and unit as `wwa_os_time_us`.
2. The stat fast path refuses to trust a record whose mtime is within 100 ms of
   *now* and re-hashes the file instead. The filesystem stamp is only as fine
   as the clock that wrote it (~15.6 ms on NTFS), so a file touched inside that
   window may legitimately carry a stamp equal to the recorded one. Git calls
   these entries "racily clean" and handles them the same way.

Cost of the guard: a full re-read of files modified in the last 100 ms — in
practice the one or two files the user just edited.

## 4. Thread safety

The manifest database was read and appended from all build workers with no
mutual exclusion, while `forge_manifest_append` can reallocate the record
array. A reader holding a pointer into the old array during a concurrent grow
reads freed memory. The database is now behind a mutex. It is off the hot path:
one lock per node, not per byte hashed.

## 5. Freestanding ABI shims

Linking `std_selftest` failed with `undefined reference to memcpy`. With
`-nostdlib` and no libc, nothing provides the four routines the C ABI reserves
for the compiler's own use (aggregate copy, struct init, large stack clears) —
GCC emits calls to them regardless of `-fno-builtin`. `stdmem.c` now defines
`memcpy`/`memmove`/`memset`/`memcmp` as one-line forwarders to the `wwa_*`
implementations. They contain no loops, so no compiler can recognise them into
a self-call.

## 6. Measurements (this machine, GCC 16.1.0, `-O3`, 58-node graph)

| scenario | executed | wall |
|---|---|---|
| cold build (after `clean`) | 58 | 4.1 s |
| warm no-op | 0 | 1.6–17 ms |
| edit one std source | 8 (1 obj + 7 links) | 2.6 s |

`forge selftest`: 6/6 pass, including the two new assertions below.
`std_selftest`: 340,375 checks, 0 failures.

## 7. Tests added

* **Warm build converges** — after a successful build, a second build must
  execute *nothing* (`built == 0 && cut == 0`). The old test only checked that
  the warm build returned, which the stale-object bug satisfied trivially.
* **Incrementality regression** — a throwaway two-node graph (generated source
  → object) is built, rebuilt untouched, then rebuilt after the source is
  rewritten with the same byte count. Asserts execute / skip / execute. This is
  the test that would have caught both defects, and it deliberately keeps the
  edit the same size to keep the timestamp path under test.
* The manifest "latest record wins" test no longer writes a fabricated record
  for a real project file (`src/std/stdio.c`) into the live database; it uses a
  synthetic path.

## 8. References

1. A. Mokhov, N. Mitchell, S. Peyton Jones. *Build Systems à la Carte.*
   ICFP 2018. — verifying vs. constructive traces, early cutoff.
2. M. Shal. *Build System Rules and Algorithms.* 2009 (tup). — cost of
   full-graph scanning vs. change-driven rebuilds.
3. H. Esfahani et al. *CloudBuild: Microsoft's Distributed and Caching Build
   Service.* ICSE-SEIP 2016. — content hashing plus trust-after-verification.
4. Ninja manual, `.ninja_log` and command-hash based dirtiness.
5. Git documentation, `Documentation/technical/racy-git.txt` — racily clean
   index entries under coarse filesystem timestamps.
6. R. L. Graham. *Bounds on Multiprocessing Timing Anomalies.* 1969 — the
   (2 − 1/m) list-scheduling bound forge's critical-path ordering relies on.

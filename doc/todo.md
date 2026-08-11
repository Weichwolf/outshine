# Now

| | |
|---|---|
| **Working on** | Hardening — the code made pristine before more defects are chased. `AdmitMesh`'s `Absent` arm is in flight and lands first |
| **Scope** | `doc/requirements.md`: **1382 features, 218 ticked, 1164 open** · `doc/bugs.md`: **53 defects** |
| **Last accepted** | The telemetry carries the eye — `eyeTravelM` 0 → 501.392 m, both counting identities structural (`da48351`) |

**Bugs come before requirements.** A defect in `doc/bugs.md` outranks any open line in
`doc/requirements.md`, and a round that touches a file with a recorded defect in it fixes that defect
in the same round. Nothing is ticked while the thing it names is broken.

## The phase order, from the owner

**1 · Hardening → 2 · Telemetry and logs → 3 · Bug hunting.** Each phase makes the next one cheap:
hardening makes a failure **loud** instead of silent corruption; telemetry makes a loud failure
**visible** in the record rather than in someone's session; and only then is hunting an act of reading
rather than of guessing. Reversed, every hunt pays to rediscover what the instrument should have said —
which is what three rounds cost today on a streamer that was never broken.

## The standing order, from the owner: pristine first

Measured 2026-08-11 over 33 335 lines: ownership and lifetime are **strong** — zero raw `new`/`delete`
in C++, zero `reinterpret_cast`, 48 `unique_ptr` sites, `-Wall -Wextra -Wpedantic -Werror` on both
toolchains, `STACK_OVERFLOW_CHECK=1`, `NDEBUG` never defined so asserts survive.

Bounds and failure handling are **weak, and weak in exactly the place wasm punishes**: 982
`operator[]` against **0** `.at()`, 28 asserts (one per 1 190 lines), `-sABORTING_MALLOC=0` with a
single caller of `core/io/Heap.h` in the whole tree, and `SAFE_HEAP`/`ASSERTIONS` unused.

**Why this is not a style question.** Natively an out-of-bounds index or a null dereference usually
segfaults — loud and immediate. In wasm32 address 0 is ordinary linear memory and an index inside the
296 MB heap is a legal access, so **the same defect that crashes the native oracle corrupts silently in
the browser.** The platform we ship on removes the safety net the code is implicitly leaning on, and
silent corruption in a build loop is what "the client freezes" looks like.

Hardening comes before further defect hunting. Bugs already recorded stay recorded.

## Now, in order

1. **`AdmitMesh`'s `Absent` arm is a terminal state that never terminates.** `TilePool::Poll` erases
   from `Done_` unconditionally and leaves the key in `Posted_`, so every later ask returns `Pending`
   — and `World::AdmitMesh`'s `Absent` arm increments a counter and returns, leaving `nd.haveMesh` at
   0, so the leaf never leaves `TargetTot`'s unready set. **Fixing the pool alone does not lift the
   loading screen.** A DEM hole is an infinite load. **Done when** a scenario over a known hole reaches
   `Resident()` and the loading screen clears, with the hole drawn as whatever the coarse ancestor has.
2. **The counters that diagnose a stuck load break during a stuck load.** `core/io/Telemetry.h` has no
   `Push(long long)`, so seven counters are cast to `int` at every site. `meshWanted` reaches 2 029 402
   in 11 s of load and `poolRepeats` 2 069 319 — `INT_MAX` is **3.2 hours** away, and 3.2 hours of
   loading is exactly what item 1 produces. Both published identities break with them. **Done when** a
   run past 2^31 keeps both identities.
3. **`eyeTravelM` counts a teleport as walking.** `Moved` has one input and cannot tell a step from a
   jump; `Walker::Reset` on `R` adds the whole distance back. Walk 500 m, press `R`, and the record
   reads 1000 m travelled and 0 m displaced — the same row a 500 m circle writes, which is the one case
   the column exists to separate. **Done when** a discontinuity is spelled at the call site that causes
   it (`Restood(Stance)` beside `Moved(Stance)`) and the teleport case is distinguishable in the CSV.
4. **`SceneRunner` converts a declared metre wrongly**, confirmed to 5×10⁻⁵ m: it treats
   `camera.eastM` on `kMPerDeg` where the ellipsoid gives `N cos φ · π/180`, so `demo/ring` runs
   **18.8 m long over 9 km** and every declared motion is off by ~0.2 %. **Done when** a declared
   distance and its `eyeTravelM` agree to the frame-sampling residual alone.
5. **The latency nobody measures.** Eye travel between a tile entering the target cut and its mesh
   becoming drawable. This is what `todo`'s old walk gate should have been testing, and no column
   carries it. The pool is CPU-bound in the mesh build — `meshCpuMsPerTile` 237.29 over four threads is
   16.9 tiles/s against `httpMsPerGet` 4.45 — so latency, not throughput, is the quantity. **Done when**
   the arrival inequality of `requirements.md` §0.7 can be evaluated from a run.
6. **Eviction.** Fields grow monotonically and their unit of removal does not exist. **Done when**
   `tilesEvicted` rises under a bound and a 2 km walk holds its budget.
7. **The silent-success class**, all three sites (`bugs.md`, first section). **Done when** the trim of
   an absent roof covering does not compile.

**Struck, 2026-08-11:** *"Nothing streams during play"* — retracted. A rung's ring radius is
`span(z)·f/kEdgeTau`, giving 0.0184 tiles/s at walking pace; the 46 s that founded the defect covered
64 m and predicted 0.85 tiles. Two longer runs agree with the derivation. The streamer was working.

## Then, from `requirements.md`

Band 0 in order (0.1 ledger · 0.2 request and priority · 0.3 budget and eviction · 0.4 arrival ·
0.5 exhaustion · 0.6 instruments · 0.7 headroom), then the picture work: **more than one prototype
resident** — one `SetPrototype` slot is what stopped fifteen finished shrub species from being drawn —
the grass stratum as a field, overdraw, the water level, one rank per stand, occlusion between 1 m and
20 m, the night.

## Standing debt

**147 of 210 ticked lines name no file**, against the rule that a ticked line names what implements it.
Band III is worst at 43 of 45, Band II at 43 of 49. Not a round of its own: each is filled in as its
band is touched, and a line that cannot be given a file was never true.

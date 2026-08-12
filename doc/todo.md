# Now

| | |
|---|---|
| **Working on** | The declared still is not one picture — the ingest order decides a 24-px band on the horizon |
| **Scope** | `doc/requirements.md`: **1428 features, 232 ticked, 1196 open** · `doc/bugs.md`: **73 defects** |
| **Last accepted** | `[[nodiscard]]` 214/214, `default:` 3→0, no new `(void)` (`a57f213`) |

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

## Now, in order — the hardening queue, from § I.17

**Items 1 and 2 are done** (`4168e68`): `make gates` runs nine gates in 6m11s and `gates-build`
seven in 26 s; each was demonstrated red for its own reason. Native ASan over the whole client
found nothing over 10 800 frames, three runs. wasm ASan is **blocked, not refused** — it links and
cannot finish one frame in 2 040 s, and it raises `INITIAL_MEMORY` by 53 %, so it could not decide
the freeze even if it ran.

**Every "done when" below now carries `make gates` green as a clause.**

1. **The declared still is one picture.** `demo/frame` gives `buildingTris` ∈ {135 168, 134 783},
   11/9 over twenty interleaved runs, **each value produced by both binaries** — so it is the scene,
   not any round. The whole difference in the log is which vector tile lands first: same 440
   footprints, two ingest orders, 1 155 verts = 3 × 385 tris, one non-indexed triangle list present
   or absent. It shows as a **24-pixel band at the horizon**, six pixels at the 320×180 comparison
   rung, on the silhouette. **Every A/B still in this repository has been read against that noise
   floor.** Nothing else is measured on stills until this closes. **Done when** ten interleaved runs
   give one `buildingTris` and one sha256, and a gate says so.
3. **A standpoint the tile scheme cannot carry is refused by name.** `osmmesh_geo_to_tile` returns
   `OSMMESH_GEO_ERR_RANGE` above |lat| 85.0511° and writes neither output; both callers read their own
   zero-initialised locals, so the world silently loads tile (0,0). Reachable from a declared
   scenario — `Scene.cpp:65` accepts lat ∈ [−90, 90] and `World::Open` has no guard. Web Mercator ends
   there by construction, so *"every point on Earth is a valid start"* is a claim the tile scheme does
   not hold; a named refusal is the honest half and a polar scheme is the owner's call. **Done when** a
   declared scenario at 86° N refuses by name instead of drawing Null Island.
4. **The hardening ledger as a script in the tree.** The `[[nodiscard]]` line is ticked and **nothing
   holds it** — one new `bool Foo()` re-opens it silently, and the round's own scanner lives in a temp
   directory. **Done when** the eight counts are a committed script inside `make gates`.
3. **Allocation.** Seven remaining `malloc` sites through `Heap`; `core/io/HeapArray.h`. **Done when**
   `grep malloc` outside `core/io/` is 0 and a run with the heap cut until it fails ends naming the
   item and the bytes.
4. **`Span` hardening, `Sub`'s wrapping bound, `core/Grid.h`, and adoption.** **Done when**
   `Span::Unchecked` sites ≤ 12 and all at a C ABI, the 40 raw pointer+count parameter pairs are 0
   outside `world/terrain`, and **`poolMeshCpuMs / poolMeshTiles` moves under 5 %** against 398 ms
   (wasm) / 190.5 ms (native).
5. **Assertions where they earn it.** **Done when** runtime ≥ 40 with `render/stages` and
   `world/terrain` non-zero, static ≥ 30, and `ClusterCut`'s silent level clamp is gone.
6. **The producer/consumer reshape.** `RoofSurface::Roofed`, `ClusterCut::Close()`, `treebench`'s
   refusal, `BindInput`'s refusal. **Done when** the two roof gates hold for their own reasons and
   `ClusterCut`'s `assert(Closed_)` is **deleted because unreachable**.
7. **The hardening ledger** — one script, eight counts, in the record. **Done when** "pristine" is a
   diff rather than an opinion.

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

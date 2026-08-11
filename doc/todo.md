# Now

| | |
|---|---|
| **Working on** | Hardening, item 1: `make gates` — one target whose green is the precondition of a commit |
| **Scope** | `doc/requirements.md`: **1418 features, 227 ticked, 1191 open** · `doc/bugs.md`: **68 defects** |
| **Last accepted** | `Absent` terminal, only a declared refusal mints it; counters 64-bit on wasm (`1424214`) |

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

1. **`make gates`.** One target running the five `verify-*` plus the declared sanitised runs plus the
   runtime refusals, one line per gate, non-zero on any failure. **Its green becomes a clause in every
   later "done when"** — a gate nobody runs is not a defence, and `verify-counters` earned that point
   today by failing against the type it was written for. **Done when** it exists and one deliberately
   broken gate turns it red.
2. **The declared sanitised runs.** `make walk-asan` (`address,undefined`), `make wasm-asan`
   (`address`). Measured: wasm ASan 2.84×, native ASan 3.83×, native UBSan 9.84× — and native ASan
   lands the oracle's CPU on the browser's unsanitised speed, so it concedes nothing new. `SAFE_HEAP`
   is `REFUSED` at 6.20× having failed to catch what ASan caught at half the price. **Done when** both
   run `demo/walk-500` clean and the native one is inside `make gates`.
3. **`[[nodiscard]]` sweep and `default:` removal.** 38 → 134, `world/` 5 → 29, `render/` 0 → 12;
   `default:` over house enumerations 5 → 0. **Done when** those counts hold **and the `(void)` count
   is published beside them**, because that is how this sweep gets faked.
4. **Allocation.** Seven remaining `malloc` sites through `Heap`; `core/io/HeapArray.h`. **Done when**
   `grep malloc` outside `core/io/` is 0 and a run with the heap cut until it fails ends naming the
   item and the bytes.
5. **`Span` hardening, `Sub`'s wrapping bound, `core/Grid.h`, and adoption.** **Done when**
   `Span::Unchecked` sites ≤ 12 and all at a C ABI, the 40 raw pointer+count parameter pairs are 0
   outside `world/terrain`, and **`poolMeshCpuMs / poolMeshTiles` moves under 5 %** against 398 ms
   (wasm) / 190.5 ms (native).
6. **Assertions where they earn it.** **Done when** runtime ≥ 40 with `render/stages` and
   `world/terrain` non-zero, static ≥ 30, and `ClusterCut`'s silent level clamp is gone.
7. **The producer/consumer reshape.** `RoofSurface::Roofed`, `ClusterCut::Close()`, `treebench`'s
   refusal, `BindInput`'s refusal. **Done when** the two roof gates hold for their own reasons and
   `ClusterCut`'s `assert(Closed_)` is **deleted because unreachable**.
8. **The hardening ledger** — one script, eight counts, in the record. **Done when** "pristine" is a
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

Type: feature
Area: render
Tags: oracle, khronos, perf

**I.26.11 Two hundred cases, so the suite is generated rather than typed**

*Owner's calibration, 2026-08-12: **"I expect hundreds of tests."** That is a design ruling, not a
length target. A suite of ~200 cases has failure modes a suite of ~30 does not, and all three of them
are structural: boilerplate that stops anyone adding case 31, an oracle cost that makes the suite
unrunnable, and empty cases that sit green. **This section is the answer to all three, and it is
deliberately not a list of 200 lines** — a hand-typed enumeration would have holes and no way to prove
it does not.*

**The count, derived rather than asserted.** Core glTF behaviours ≈ 80 · extension cases ≈ 28, one per
ratified extension we require plus its edge cases · technique cases ≈ 70, kind B of § I.26.8 at several
cases each · integration scenes ≈ 20. **≈ 200 for the renderer stage alone**, before vegetation and
buildings have their own.

- [ ] **The axes are declared and the enumeration is mechanical from them**, which is what makes 200 provable instead of typed: **feature** (the § I.26.8 matrix) × **kind** (A fetched · B authored) × **instrument** (boundary p95 · depth · radiance · frame time · by eye) × **subject class** (opaque ≥ 1 px · sub-pixel present) × **motion** (still · moving). A case is a point in that space and a hole in the enumeration is a query, not an act of memory
- [x] **Adding a case is adding a directory, never writing a file.** Roughly 90 % of render cases ask one question — *load this glTF, place this camera, render, compare against the oracle at these thresholds* — so the C++ is **identical across all of them and only the manifest differs**. One parametrised parity test, instantiated by the harness once per test directory *(`test/run.sh` `LayerCases`, `test/harness/shared/render/Parity.cpp`)*
- [x] **Still one process and one real verdict per case**, which is `test/run.sh`'s existing requirement and is not relaxed: the harness enumerates the directories and runs the parity binary once per directory with the directory as its argument. What is shared is the **code**, not the process, and a crash in case 137 fails case 137 *(`test/run.sh` `Judge`)*
- [ ] What the render runner reads and nothing else: the `.gltf`, the resolved camera, the resolved recipe, the subject class and the resolved thresholds — from the declaration and the defaults (§ I.26.10). **It contains no scene-specific branch at all**; a case needing one is not a render case
- [ ] **A case whose question is not "does it match the oracle" is not in this suite at all** (§ I.26.9). *This line first read "it writes its own `.cpp`" — an escape hatch inside the render suite, which is exactly how a scoreless case ends up in a suite whose contract is a score. The escape is a **different suite**, not a different file.* § I.26.3's time contract is arithmetic against `n/fps` and is a **unit** test; § I.26.7's forest is a frame-time distribution and is a **scenario** test
**The oracle cache, justified against the numbers this round measured rather than against a feeling.**
*The line here first read "200 Cycles renders at 720p is **hours**". **That is contradicted by this
round's own measurement** — 200 × 2.087 s = **417 s ≈ 7.0 min** — and a claim a round measures and then
contradicts is the precise failure the bug tasks in `board/` records twice already. Corrected, with every number
labelled.*

| | value | measured or derived |
|---|---|---|
| warm frame, Metal, factory cube, 720p, 128 spp | **2.087 s** | **measured** (§ I.26.4) |
| cold Metal kernel compilation, once per cache generation | **200.9 s** | **measured** |
| 200 still cases, warm, cube-cost | **417 s ≈ 7.0 min** | **derived** from the two above |
| first run of those 200 on a cold cache | **≈ 10.3 min** | **derived** |
| one case alone, cold against warm | **203 s against 2.1 s ≈ 100×** | **derived** |
| film segment, 240 frames, cube-cost | **8.4 min** | **derived** |
| pavilion, forest, scan-derived subject | **unknown multiple of the cube** | **projection — not measured**, and one timed frame is the tool |

- [ ] **The cache's honest justification is three things, and bulk stills are not among them**: the **cold-start cliff**, where one case costs 203 s instead of 2.1 s and every fresh checkout, every container and every Blender upgrade pays it · the **film**, whose frame count multiplies directly and whose cost scales with a sequence rather than a frame · and **scene cost**, which is a projection and is labelled as one — the 2.087 s subject is a six-quad cube, the cheapest that exists, and the pavilion and the forest are unmeasured
- [ ] **For the two hundred stills the cache is a convenience and the design says so.** Seven minutes warm is affordable; it is not the reason the cache exists, and inflating it into one would make the next round distrust every number beside it
- [ ] **The strongest reason is not performance at all: a cached oracle keyed by recipe and release cannot change underneath a comparison.** Without it, a reference re-rendered per run can move for a reason nobody chose — a driver, a sampler, a point release — and every difference becomes unattributable. *That is a correctness argument, and it would justify the cache at zero render cost*
- [ ] Hash-keyed in the content store § I.22 already has, computed once, invalidated only when the recipe or the Blender release changes. **That is why the release is in the key** and not merely on the row
- [ ] **The tier split, stated with what the fast tier actually costs rather than with an assertion that it cannot afford one.** With a warm cache the fast tier costs **2.087 s per oracle it would have to render, derived** — real but survivable, so *cannot afford it* was the wrong reason. The right one is that **the fast tier must not invoke Blender at all**: an external binary on the fast path is an unbounded dependency whose cost is a **100× cliff** the moment the kernel cache is cold, and a tier whose runtime depends on whether somebody upgraded Blender is not a fast tier. So a fast-tier cache miss is a **named refusal**, never a silent skip and never a render nobody asked for; the `device` tier renders and populates
- [ ] **A cold cache is a declared, separately-invoked run** with its cost published — `N × t_frame` plus the **200.9 s measured** Metal kernel compilation once (the bug tasks in `board/`). For 200 cube-cost stills that is **≈ 10.3 min derived**; for the film and the complex scenes it is a projection until one frame of each is timed, and the run prints which of its numbers are which
- [x] **What stops a generated suite going hollow is the empty-image guard and not a count of declared numbers** (§ I.26.10): under defaults, declaring nothing *is* a complete specification, so "a directory with no acceptance numbers is a refusal" would refuse every correct minimal case. The guard that actually holds is **zero coverage on either side is a failure, an absent or failed oracle is a failure, and the trailer distinguishes agreed from nothing-to-compare** *(`test/harness/shared/render/Parity.cpp`, `test/harness/shared/render/Acceptance.h` `kCoverageFractionMin`)*
- [ ] **The suite publishes its own coverage of the matrix as a count** — features with a case, features without, cases with each instrument — so *"every feature KCD or GTA 5 uses has a basic case"* is a number somebody can read rather than a claim somebody made. Without it, 200 directories are 200 opportunities for a feature to have quietly no case at all

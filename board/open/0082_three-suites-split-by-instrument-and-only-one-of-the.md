Type: feature
Area: render
Tags: oracle, khronos, perf

**I.26.9 Three suites, split by instrument — and only one of them mirrors `src/`**

*Owner's ruling, 2026-08-12: **separate the tests into declarative render tests, declarative scenario
tests, and unit tests.** It governs the two sections below, and it supersedes their `<area>` mirroring
for two of the three kinds. **The split is by instrument, not by shape** — that is what makes it hold,
because two declarative suites look alike and are decided by entirely different questions.*

| Kind | Declaration | What decides it | Mirrors `src/` |
|---|---|---|---|
| **render** | the `.gltf` | agreement with the oracle — boundary p95, radiance median | **no** — organised by feature |
| **scenario** | the scenario: camera × clock × world-or-studio (§ I.25) | frame time p50/p95/p99 over motion · determinism · residency · streaming · memory | **no** — organised by declared run |
| **unit** | the code | a stated invariant, nothing rendered | **yes, exactly** |

- [ ] **The placement rule, so this does not drift: *what would fail this test?*** — *"our pixels disagree with Cycles"* is a **render** test · *"the frame floor broke, the run was not deterministic, or memory grew"* is a **scenario** test · *"the code computed the wrong thing"* is a **unit** test. **A case that seems to fit two is testing two things and is split**
- [ ] **Only the unit tree mirrors `src/`, and it must, because its organising axis *is* source location.** It is the tree that carries `CLAUDE.md`'s property: each directory compiles with its own include set, so a name it must not reach has no spelling and a breach is a **compile error**. Every unit test is a continuous proof that its layer's include set is exactly what it claims
- [ ] **The declarative suites carry none of that and restoring the mirror over them would destroy it.** A render case links the whole library by construction — it needs the reader, the renderer and the readback at once — so a mirrored `render/core/` test directory would compile with a *wider* include set than `test/unit/core/` and the mirror would stop meaning anything. *Written down because "restore the mirror everywhere" is a tidy-looking change that dilutes a proof into a convention, and the dilution is invisible afterwards*
- [ ] The two declarative suites are organised by what they declare: **render by feature** — the § I.26.8 matrix is the directory structure — and **scenario by declared run**, so a run is found by the thing it runs rather than by the source file it happens to exercise
- [ ] **`GrownBarkIsAClosedMesh` and its whole class are unit tests and keep the mirror**: closed, wound, unit-normal and in-range are decidable geometric invariants over a generated mesh, with no reference and nothing rendered. *They are the shape the unit tree is for, and they are already in the right place — this line exists so a later round does not re-file them as render tests because they concern geometry*
- [ ] **§ I.26.3's time contract is a unit test**, not a render one: *for every `n`, the engine's animation time equals `n / fps` exactly, and equals the glTF sampler input at the corresponding keyframe*. Arithmetic, no oracle, no pixels — and it keeps the mirror at `test/unit/clients/`

**The re-filing this ruling forces, and the forest was not the only one.**

- [ ] **The forest rung moves to the scenario suite** (§ I.26.7), and it was filed wrong: its instrument is **frame time over a moving camera plus judgement by eye**, and there is **no oracle comparison in it anywhere** — Blender's render is a reference *for the eye* and never a number. A case with no score, inside a suite whose entire contract is a score, is the hollow case the empty-image guard exists to catch, and it would have been the first one
- [ ] **Re-checked across the whole matrix, seven more move**, by the same test — none of their acceptances is agreement with the oracle: **overdraw** (fill cost at declared depth complexity) · **LOD transition** (a pop at a named distance, only visible in motion) · **TAA** (ghosting against softening, and § I.26 already runs every parity rung with TAA off, so it has no oracle counterpart by construction) · **particle determinism** (a fixed seed pinning its own output) · **crowds of skinned characters** (a skinning-throughput claim) · **large-scale terrain** (LOD, cascade range and streaming together) · and the **forest density sweep** (where 720p60 breaks)
- [ ] **The film rung splits in two rather than choosing**, because it genuinely tests two things: the **per-frame difference series against the stored oracle** is a *render* case, and the **discontinuity test plus the frame-time distribution over the same segment** is a *scenario* case. *They share a scene and a camera path and answer different questions, and merging them would produce a verdict nobody can attribute — which is the same objection as a single blended score, one level up*
- [ ] What stays in the render suite is everything whose failure is *our pixels disagree with Cycles*: the spine's twenty-one rungs, every kind-A format case, and the kind-B technique cases with an oracle counterpart — cascades, SSR, water, decals, volumetrics, motion blur, depth of field, bloom and tonemapping, wet surfaces, hair and cloth as geometry
- [ ] **Each suite states its own verdict shape once, and they are three different shapes**: render is *every named metric within its own threshold and direction* (§ I.26.10) · scenario is *the frame-time distribution within its declared floor, the run bit-identical across two invocations, residency and memory within declared ceilings* · unit is *the invariant held*. **A suite that borrowed another's verdict shape would be reporting a number that does not decide it**

Type: bug
Area: render
Tags: scope

**`BenchGround` is a catalogue row named for a harness that no longer exists — **Band 2****

`src/render/plan/RenderCatalogue.h` carries `{Stage::BenchGround, Provenance::Content, PassKind::Raster,
"benchGround", …}`, reading `ShadowAtlas`, `IrradianceBuffer` and `CascadeUniform` and contributing to
the three scene targets. **`git grep BenchGround` over `src/` and `test/` returns two files: the
catalogue row, and the arm of `Renderer::Executable` that returns `false` for it.** No implementation, no
consumer, no test — *and the enumeration is the grep over both trees, so this is a count.*

**The name refers to something deleted.** A bench floor is the ground plane a subject bench stood its
subject on, and the benches went with the browser-era clients — `SubjectBench`, `TreeBench` and the walk
bench are all gone. **So the row is named for a harness convenience that no longer exists, and it was a
harness convenience before that.**

**Why it is a defect rather than dead weight**, and it is the same sentence as the entry above it: **the
catalogue is the engine's capability claim.** A row asserts *this engine can draw this*. `BenchGround`
asserts that the engine can draw a bench floor — a fixture for a test that is deleted — and it is the
fifth kind of content noun in a layer forbidden them: neither medium, nor body, nor surface class, nor
mechanism.

**The harmless explanation, sought.** *It is one row and it costs nothing* — it costs the two things a
capability claim costs: it is counted in `kStageCount`, and it is one of the eighteen a reader has to
check before believing any row. *It marks a place for a future bench* — a bench is a test, and § I.28
already rules that what a test needs is the compositor's draw list rather than the renderer's type
system; that is the same finding that removed the five geometry units.

**Right:** delete the row. **Fixed when** `git grep BenchGround` returns nothing, and `kStageCount` falls
by one with its `static_assert`s re-proved — which is § I.27's own statement of what removing a row costs.

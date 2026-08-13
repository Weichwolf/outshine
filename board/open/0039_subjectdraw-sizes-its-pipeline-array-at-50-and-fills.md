Type: bug
Area: render
Tags: scope

**`SubjectDraw` sizes its pipeline array at 50 and fills 30, and the invariant is held in a different method — **Band 2****

`render/stages/SubjectDraw.h:288-290` — `kPipelines = kVertexLayouts * 2 * kSurfaceKinds` = **5 × 2 × 5
= 50**. `SubjectDraw::Configure` (`.cpp:618`) iterates `{Opaque, Masked, Blended}` and builds **30**;
`SurfaceKind::ThinTransmissive` and `SurfaceKind::Refractive` are never built, so **20 of 50
`OwnedPipeline` slots are null for the life of the object**. `PipelineAt` (`.cpp:681`) addresses all
fifty and refuses nothing, and `Encode` (`.cpp:968-970`) binds `Pipelines[wantedPipeline].Get()` with
no check.

**The harmless explanation, sought — and it holds, which is why this is Band 2 and not Band 1.**
`SetMaterials` (`.cpp:819-826`) refuses a slot whose `SurfaceState::Kind()` is transmissive or
refractive, by name and with a sentence, and clears the table; and nothing in `src/` writes
`Material::Transmission` at all — `grep` outside `core/Material.h` and `core/SurfaceState.h` returns
nothing. **The null bind is therefore unreachable today.** It is not a latent crash to be reported as
one.

**What is left is still a defect.** The sizing expression states a capacity the constructor
contradicts, and the only thing between an index function and a null pipeline is a guard in an
unrelated method — so the rule *"this unit draws three of the five kinds"* is **written down and
enforced at run time** where the array's own type could carry it. `kSurfaceKinds = 5` is a count of the
enumeration, not a count of what this unit draws, and using one for the other is `ES.45`'s complaint
about a constant standing for something it is not.

**Right, and § I.28 makes it moot:** the pipeline key is `(VertexLayout, SurfaceState)`, `SurfaceState`
is already `constexpr`-derived from `Material` (`core/SurfaceState.h`, `StateOf`), so the set of
pipelines is the set of distinct keys the compiled draw lists contain and the count is an output of the
plan compiler — as the pass count already is. **Fixed when** no array in this file is sized by an
enumeration's cardinality, and a surface kind this unit cannot draw is refused where the *plan* is
compiled rather than where a material table is set.

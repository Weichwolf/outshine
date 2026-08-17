Type: issue
Area: corpus
Tags: scope, oracle, khronos, instrument
Supersedes: 1168

**The three phases, and phase one enumerated**

**The owner, verbatim:** *1. pass the khronus test corpus - 2. develop the correct generator -> compositor
-> render pipeline with scenario engine/loader - 3. render movies with outshine using scenarios*

**This supersedes `board:1168`, which superseded `board:1159`.** Three ordering statements, each keeping
its own evidence: `1159` carries the census, `1168` carries *still to animated* and the measurement that
the scenario loader **loads** while nothing drives a draw from a time. **Neither is reopened.** `1159` is
settled by that chain and needs no further action — a second superseder of it would be a fourth ordering
statement about the same question.

**What the phases decide that neither of ours did.** Phase 1 has a **countable finish line against
something outside this tree**; both of our orderings had a direction. And the animated assets the owner
named earlier — `BoxAnimated`, `InterpolationTest`, `AnimatedMorphCube` — **are Khronos sample models**,
so `board:1128` is not a detour before phase 1, it is inside it. The scenario loader is phase 2. **A movie
is frames, so still → animated is phase 1's own tail and phase 3's precondition.**

## Phase 1 enumerated, from the pinned upstream

`Models/model-index.json` at the pin every manifest in this tree already cites,
`glTF-Sample-Assets @ 2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf`:

| | |
|---|---|
| **models in the index** | **148** |
| tagged `core` | **72** |
| tagged `extension` | 73 · `testing` 111 · `showcase` 24 · `pbrtest` 16 · `issues` 7 |
| **our render suite** | **35 cases**, over **22 distinct Khronos models** and 11 subjects we generate |
| **all 22 resolve in the index** | yes — no drift against the pin |
| **`core` models with no case** | **53 of 72** |
| **within the picture bound** | **21 of 35** (`board:1144` moved it from 20) |
| Khronos criteria met | 30 met · 5 red |

**THE COUNT ITSELF NEEDED CHECKING AND THAT IS WORTH RECORDING.** The same reader returned **148**
enumerated, then **228**, then **244** for one file. 148 is taken because the enumeration runs
`ABeautifulGame` → `XmpMetadataRoundedCube` and **an independent query names those same first and last
elements** — a truncated list would not end at the true last. The other two were arithmetic over a list
the reader had in front of it. *Three readings, two wrong, settled by checking the endpoints rather than
trusting a total.*

## What phase 1 is NOT, and the exclusion is a declared list

**148 is not the target.** The scope is what this engine has a term for, and every exclusion carries its
reason rather than being filtered silently. The bands, with counts, to be resolved into a per-model list:

- [ ] **`core` — 72 — is the spine**, and 19 of them have a case. This is where the finish line lives
- [ ] **`extension` — 73** — admitted only per extension. `KHR_lights_punctual`, `KHR_materials_unlit`
  and `KHR_materials_emissive_strength` are already read by this tree; **transmission, volume,
  iridescence, sheen, anisotropy, clearcoat, dispersion, specular-gloss, Draco, meshopt and instancing are
  terms this engine does not have**, and a case for one measures our ambition rather than our correctness
- [ ] **`pbrtest` — 16 `Compare*` grids** — 9 are extension-only and go with their extension; the 7 `core`
  ones are a separate decision, because a comparison grid is a *picture of a parameter sweep* and its
  verdict shape is not the picture bound's
- [ ] **`issues` — 7** — models Khronos itself marks as problematic. **Each needs its own reason**: a
  known-bad asset failing is not our defect, and admitting one without reading its issue is how an
  unfixable case enters the suite
- [ ] **Skinning and morph — `SimpleSkin`, `RiggedSimple`, `RiggedFigure`, `BrainStem`,
  `RecursiveSkeletons`, `AnimatedMorphCube`, `MorphPrimitivesTest`, `MorphStressTest`, `SimpleMorph`** —
  in scope and unbuilt; they are `board:1128`'s own tier and they are `core`

**`board:0078` is phase 1's feature and no second one is filed beside it.** It is the asset matrix with a
dependency order and it already ranks these rungs. What it does not carry is the **repair half** — the
mip and filter thread (`1130` `1132` `1150`), the coordinate term (`1151`), identity routing (`1144`,
landed) and the tangent assets (`1126` `1127`). **Phase 1 is those repairs plus that expansion**, and
nothing here restates either.

## Phase 1's Done when, in one falsifiable sentence

**Every Khronos model in the declared in-scope set has a render case, and every one of those cases is
within the picture bound or carries a declared reduction naming why its oracle cannot decide it** — with
the two counts published side by side and neither quotable as the other.

**Phase 2 and phase 3 are named and not decomposed.** Phase 2 is the front page's own decomposition plus
the scenario engine — `board:0055` `0108` `0113` `1161` `1162` already stand for parts of it. Phase 3 is
movies from scenarios and has no item, correctly: **a phase nobody is working does not need tasks, and
filing them now is the inventory defect two rounds were spent undoing.**

**Done when** the in-scope set is a declared list with a reason per exclusion, the gap between it and our
35 cases is a number on the board, and phase 1's sentence above is either true or names which case fails
it.

## The order WITHIN phase 1, and the 53 split by what their verdict depends on

**The 53 uncovered `core` models are not one population**, and treating them as one is what makes
*repairs first* and *expansion first* look like the only two answers. Classified by name and tag against
the pin — **a first pass that each model's own manifest must confirm**:

| band | n | what decides it |
|---|---|---|
| **animated · skinned · morph** | **16** | blocked entirely — nothing drives a draw from a time |
| **plain geometry, no texture** | **13** | coverage and depth, which this suite already decides well |
| **textured, rides an open repair** | **9** | `board:1150` and `board:1151` decide these before they are worth adding |
| **heavy or effectively extension** | **9** | `Sponza` is refused on licence; `MetalRoughSpheres` is named unreducible in `board:0088` |
| **`Compare*` grids tagged `core`** | **6** | a parameter sweep's verdict shape is not the picture bound's |

**1 — `board:1169`, and it is also the single biggest reducer of the 53.** `BoxAnimated` is one model and
the link it forces into existence unblocks **16** — nearly a third of the gap, and the only band where
*every* member is blocked by one missing consumer rather than by an open question. It is the owner's
*still to animated* and the largest arithmetic move available, which is a rare agreement.

**2 — the 13 plain-geometry models, and this is where I attack the prior.** *Adding 53 against an oracle
we cannot adjudicate multiplies an unsolved problem* is right about the textured 9 and **wrong about
these 13**: `Box`, `BoxInterleaved`, `BoxVertexColors`, `Cube`, `SimpleMeshes`, `SimpleSparseAccessor`,
`TwoSidedPlane`, `VertexColorTest`, `UnicodeTest` and their neighbours carry no texture, no normal map and
no minification, so **not one of them touches `1150`, `1151`, `1126` or `1127`.** They are decided by
coverage and depth, where the suite's record is 30 criteria met against 5 red. **They are also the only
cheap way to find out whether the engine's problems are the four known threads or whether there is a
fifth** — every one that lands green is a sample that says the failures are concentrated, and every one
that lands red is a finding worth more than another repair round.

**3 — `board:1150` then `board:1151`**, in that order: the appearance recipe decides *what the oracle is
allowed to adjudicate* and the coordinate term decides *how much disagreement is admissible*, and the
second is meaningless while the first is open. **They gate the textured 9, and `1150` also closes
`board:1130`'s last open question.**

**4 — `board:1170` and `board:1166`, and they are not housekeeping here.** At 129 MB a case the remaining
53 cost **6.8 GB** against 53 GiB free; pruned they cost ~50 MB. This is the item that makes bands 2 and 3
affordable rather than a thing to do afterwards.

**5 — `board:1128`'s remaining decomposition** — interpolation, skinning, morph — which is band 1's tail
and is written once `1169` has proven the mechanism it all shares.

**What is NOT in phase 1's order**: the `Compare*` grids and the heavy nine, both of which need a decision
before they need work, and both of which are `board:1172`'s declared-scope list rather than a case.

## Re-priced for the extension band, and the ordering CHECKED rather than agreed

**The denominator moves from 72 to the index**, on the owner's ruling that glTF 2.0 **with extensions** is
the definition (`board:1172`). **148 models**, of which 73 carry `extension`; the in-scope set is every
model whose extensions are ratified or multi-vendor-and-needed, minus per-asset exclusions.

**The corpus arithmetic, and `board:1170` is what makes it expressible rather than a collision:**

| | at today's 129 MB a case | pruned to pictures |
|---|---|---|
| the remaining 52 core | 6.7 GB | ~50 MB |
| **the whole in-scope index, ~147 cases** | **~19 GB against 53 GiB free** | **~150 MB** |

Cycles is still not the constraint: p50 0.60 s a render, so a cold rebuild of the whole index is **on the
order of two to four minutes**, not hours.

**THE ORDERING HOLDS AT THE FRONT, AND I CHECKED IT RATHER THAN AGREEING.** Every model in the extension
band needs a **renderer term that does not exist** — a lobe, a scene-colour read, a decoder — so not one
of them is blocked by a missing *case*; they are blocked by missing *implementation*. **A band that cannot
start cannot be first.** `board:1169` still unblocks 16 core models through one missing consumer, and the
13 plain-geometry models still touch none of the open repairs.

**One thing DOES move, and it is the point of checking.** `board:1150` and `board:1151` gated **9** core
textured models before; the extension band is overwhelmingly textured and PBR, so those two now gate
**9 plus most of 73**. **They rise above the corpus expansion's tail** — no longer *third after the plain
thirteen*, but the thing that decides whether any textured case beyond the current 35 can be adjudicated
at all. The revised order within phase 1 is therefore **`1169` · the plain 13 · `1150` · `1151` · the
prune · then the extension families in `board:0078`'s order**, and the extension families are gated by
implementation rather than by corpus work.

## The owner set the finish line, and it is stricter than what stood here

**Every one of the 148 models green, on BOTH published counts** — *criteria met* AND *within the picture
bound*. Not one of them, not either of them: both, for all.

**What that strikes.** The phase-1 statement allowed *inside the bound OR carrying a declared reduction*
as an end state. **It does not any more.** A declared reduction is now an OPEN case with a named cause,
never a finished one — the reduction ladder keeps its place as the route a round takes, and loses its
place as a place to stop.

**Licence is not a filter.** `board:0074` measured 13 further SPDX identifiers at the pin, nine of them
restrictive, and asked whether those models are in scope. **They are: the models are fetched and
rendered, never redistributed**, so the allow-list is an audit of what we consume and not a gate on what
we must prove.

**148 is models and the case count is larger.** Three directories in this tree already show why —
`Cameras`, `SimpleTexture` and `MaterialsVariantsShoe` each declare two cases over one model. The target
is **at least 148 cases over exactly 148 models**, and a model with two cases needs both green.

**What the finish line does NOT claim, stated here so it is not read into it later.** The corpus is
**one subject, one camera, a still**. Green over all 148 is a complete statement about `src/gltf` and
very nearly one about `src/render` — and it is nearly silent about the compositor, whose work is
culling, budget quantisation, the ladder, streaming, residency and eviction, none of which a still can
turn red. It says nothing at all about 720p60, which is the `frame` suite's distribution over a moving
camera. *The fourth constraint stays the least measured of the four whatever this count reaches.*

## The licence ruling reached the preparer, and eleven models came with it

**The ruling above was recorded and not implemented.** `prep/licence.py` still refused: an allow-list of
`CC0-1.0` and `CC-BY-4.0` plus a `LicenseRef-LegalMark-*` prefix, and **eleven models refused by name** --
`Sponza`, `BrainStem`, `DamagedHelmet`, `BoxTextured`, `BoxTexturedNonPowerOfTwo`, `AntiqueCamera`,
`CesiumMan`, `CesiumMilkTruck`, `PrimitiveModeNormalsTest`, `RecursiveSkeletons`, `Duck`.

**A refusal became a note.** The licence is still read at the pin, still carried, and now printed once
per run as *recorded, not refused* -- the same shape the preparer already uses for a Blender version it
did not expect.

**The knowledge was kept and not deleted.** `REFUSED_SUBJECTS` became `NOTED_SUBJECTS` with every entry
and every reason intact, because each was established on evidence and **a deleted line is scope given
up**. If anything is ever published out of this corpus, that list is what says which models may not go.

**What it unblocked immediately**: `Duck` prepared on the first attempt after the gate came down -- and
in doing so uncovered `board:1370`, a stale object reference in the preparer that no reachable subject
had ever triggered. *A gate does not only block what it names; it hides everything behind it.*

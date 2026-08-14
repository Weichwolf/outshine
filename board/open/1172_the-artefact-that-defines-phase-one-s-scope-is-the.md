Type: bug
Area: corpus
Tags: oracle, khronos, instrument, scope

**The artefact that defines phase one's scope is the one artefact nobody fetched**

Every subject in this corpus is fetched, digested against a declared SHA-256 and kept in the content
store. **`Models/model-index.json` is not.** Phase 1's denominator — `board:1171`'s **148 models, 72
`core`, 53 uncovered** — came from a **live fetch**, and it is not reproducible offline: no object under
the content store contains `XmpMetadataRoundedCube`, searched **by content** rather than by name, because
store objects are named `derived_key(kind, recipe)` and not by their bytes.

**The pin is real and tracked** — `glTF-Sample-Assets @ 2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf`, cited
by every Khronos manifest — so the *subjects* are pinned and the *inventory over them* is not.

**Why this is worse than an ordinary missing digest.** The same reader returned **148**, then **228**,
then **244** for that one file. 148 was taken only because an independent query named the same first and
last elements, so a truncated list was ruled out. **Had the first total been taken, phase 1's denominator
would have been wrong by 80** — and nothing in the tree could have contradicted it. *A number that decides
a phase's size, produced by an instrument that disagreed with itself twice, with no offline path to
re-derive it.*

## The repair, and the weaker option is named so it is not chosen by default

- [ ] **Fetch and digest the index like any other product** — correct, and **insufficient alone**: it
  makes the number reproducible and leaves the denominator a function of upstream. A model added upstream
  would silently change what *pass the Khronos corpus* means, and the pin would still verify
- [ ] **THE IN-SCOPE SET IS DECLARED IN THIS TREE AND CHECKED AGAINST THE PIN — take this one.** The set
  of models phase 1 must pass is **ours**, written down, one line per model with its band and, where it is
  excluded, the reason. The index is fetched and digested, and the check is that **every declared model
  exists at the pin** and that **every model at the pin is either declared in scope or declared out with a
  reason**. An upstream addition then **fails a check** instead of moving a denominator
- [ ] **The two counts stay apart**, as everywhere else here: *models declared in scope* and *models with
  a case within the picture bound*. Phase 1's sentence needs both and neither is the other
- [ ] **The exclusion reasons are the valuable half**, not the list: *this engine has no term for
  `KHR_materials_transmission`* is a statement that can be revisited when it gains one, and *Khronos marks
  this model `issues`* is a statement about the asset. A filter with no reasons is a number nobody can
  argue with

**The caveat, sought and cleared.** *Is a declared set just a second copy of upstream that will drift?* It
is a copy, and the drift is **the point**: it drifts visibly, against a digest, at a named commit, and a
check goes red. The alternative is a denominator that changes when someone else edits a file — which is
the same defect as a threshold that moves without a diff, one level up.

**Done when** the index is a fetched product with a digest, the in-scope set is declared in this tree with
a reason per exclusion, a model present at the pin and absent from the declaration is a failure, and
`board:1171`'s 148 / 72 / 53 are re-derivable with the network unplugged.

## CORRECTED by the owner's scope rule, and my exclusion list collapses from fifteen to one

**The owner, verbatim:** *outshine engine relies on glTF so every glTF 2.0 feature must be supported and
tested.*

**A `core`-tagged model uses only glTF 2.0 core features, so it cannot be excluded for what it needs.**
The only admissible exclusion is about the **asset** — a licence, a file Khronos itself marks bad — and it
is written **per asset with its own reason, never per band.**

**MY OWN BANDS WERE WRONG AND IN A WAY WORTH NAMING: I excluded fifteen models for two different kinds of
reason and only one kind was about scope.**

| what I excluded | n | corrected |
|---|---|---|
| *heavy or effectively extension* | 9 | **8 are in scope.** Only `Sponza` stays out, and its reason is **per asset**: the Khronos copy is under a Crytek EULA, which is a licence refusal and not a capability one |
| *`Compare*` grids tagged `core`* | 6 | **all 6 are in scope.** I excluded them because a parameter sweep's **verdict shape** is not the picture bound's — and that is a *criterion* question, not a *scope* question |

**Scope and verdict shape are two decisions and I collapsed them into one.** `MetalRoughSpheres` is named
unreducible in `board:0088`; that decides **how it is judged**, not **whether it is in**. The ladder for
an unfixable reference already exists — `board:0085`'s `self-describing` case, with the residual printed
and deciding nothing. **A model whose oracle cannot adjudicate it still needs a case; it needs a different
verdict.**

**So the in-scope set is: every `core` model at the pin, minus per-asset exclusions with reasons.** On
today's enumeration that is **72 minus 1 = 71**, and the gap against our 19 is **52 rather than 53**.

## Extensions are outside the owner's sentence, said here so the two sets are not read as one

**`KHR_materials_{transmission, volume, iridescence, sheen, anisotropy, clearcoat, dispersion, specular,
ior, pbrSpecularGlossiness}`, `KHR_draco_mesh_compression`, `EXT_meshopt_compression`,
`EXT_mesh_gpu_instancing`, `EXT_texture_webp`, `KHR_xmp_json_ld` and every other registered extension are
NOT glTF 2.0 core.** The owner's sentence says *every glTF 2.0 feature*, and an extension is by
construction a thing the specification does not require of a conforming implementation. **They stay out,
each on its own line with its own reason**, and `board:0079` already carries most of those reasons.

**Three extensions are already read by this tree and are a separate case again**: `KHR_lights_punctual`,
`KHR_materials_unlit` and `KHR_materials_emissive_strength`. **Implemented is not the same as required** —
they are in because a case needed them, and they need their own line saying so rather than being carried
by the core rule.

**The declared set therefore has three bands, not two**: *core, required* · *extension, implemented, kept*
· *extension, out, with a reason*. A model at the pin that falls in none of them is a failure of the
declaration, which is what makes the check worth having.

## CORRECTED AGAIN: extensions are in scope, and the line is the registry's status rather than the prefix

**The owner, verbatim:** *i think glTF 2.0 with extensions is a very good definition of what a modern game
engine must be able to process.*

**The section above is struck where it says extensions are out.** It said *an extension is by construction
a thing the specification does not require of a conforming implementation* — true of conformance, and
**not the question**. The question was what this engine must process, and the owner has answered it. *A
boundary inferred from a definition rather than drawn by the person whose scope it is.*

**AND THE PREFIX IS NOT THE LINE.** Checked against the registry rather than recalled — the glTF extension
registry's own categories are **Ratified Khronos** (`KHR`) · **In-progress Khronos and Multi-Vendor**
(`KHR` *or* `EXT`, at Proposal / Initial Draft / Review Draft / Release Candidate) · **Multi-Vendor**
(`EXT`) · **Vendor** (`ADOBE`, `AGI`, `CESIUM`, …) · **Archived**. So `KHR_` does **not** mean ratified —
a `KHR_` can be a draft — and `EXT_` does **not** mean vendor, it means *implemented by more than one
vendor*. **A prefix rule would have admitted drafts and excluded multi-vendor extensions**, which is the
opposite of what either of us intended.

**The line, per extension, with its reason:**

| status | verdict | why |
|---|---|---|
| **Ratified Khronos** | **in scope by default** | the specification's own stable set; ratification is a property of the document, not a preference |
| **Multi-Vendor `EXT_`** | **in scope when an asset we must process uses it** | more than one vendor ships it, so it is real upstream data — but nothing ratifies it |
| **In-progress (any prefix)** | **out by default, revisited on ratification** | a draft can change under a pinned corpus, and **this whole method is pinning.** Admitting one puts a moving target inside a digest |
| **Vendor (`ADOBE_`, `AGI_`, …)** | **out unless an asset requires it** | and the registry states they are *not covered by the Khronos IP framework* — the same class of consideration that already refused `Sponza` |
| **Archived** | **out, in Khronos's own words** | *no longer recommended for creating new files*; `KHR_materials_pbrSpecularGlossiness` is the case in the tree |

**Every exclusion is per extension with its reason and is revisitable when the engine gains a term for
it.** No band-wide filter, in either direction — including the ratified band, where an individual
extension may still be out for a stated reason.

**The declared set's bands become**: *glTF 2.0 core, required* · *ratified extension, required* ·
*multi-vendor, required where an asset needs it* · *out, per extension, with a reason*. A model at the pin
falling in none of them is a failure of the declaration.

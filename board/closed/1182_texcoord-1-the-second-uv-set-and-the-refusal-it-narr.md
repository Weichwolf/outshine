Type: task
Parent: 0079
Area: gltf
Tags: khronos, oracle, instrument

**`TEXCOORD_1`: the second uv set, and the refusal it narrows rather than removes**

`board:0079` row 3. **Impact 9 of 146**, tier 1, and a second reason: it unblocks **9 of
`TextureTransformMultiTest`'s 27 cells** (`board:1180`).

## The asset separates the claim, and I checked before writing the task

**`MultiUVTest` at the pin does what `TextureTransformMultiTest` did not** — this is the same trap
`board:1179` records, checked rather than assumed:

- one primitive declaring **both** `TEXCOORD_0` and `TEXCOORD_1`, alongside `NORMAL` and `TANGENT`
- **genuinely different data**: accessor 4 at `bufferView 4, byteOffset 996`; accessor 5 at
  `bufferView 5, byteOffset 1188` — separate views, not one accessor read twice
- `baseColorTexture` with **no** `texCoord` (set 0 by default) and `emissiveTexture` with
  **`"texCoord": 1`**

**So *reads set 1* and *reads set 0 twice* land the emissive image in two different places, and the case
can tell them apart.** It is **not fetched** — no `MultiUVTest` anywhere under `test/` — so this task
carries a corpus case as well as a vertex stream.

- [ ] **One residual to confirm when the case is declared, or it passes either way**: the emissive image's
  placement under set 1 must differ from its placement under set 0 by **more than the picture bound**.
  Different accessors guarantee different data, not visibly different placement. **Measure it from the two
  accessors' own values before scoring the case** — a case that cannot distinguish the defect is worse
  than no case, because it reports green about it

## What is wrong in the picture, and why this one is dangerous

**Reading set 0 twice looks plausible** — an emissive image on the wrong uv is still an image on a
surface, and nothing about it reads as a defect. **That is why the asset's two sets must differ visibly
and why the failure must be stated before the case runs**: this is the silent-success class, not a
disagreement class.

## Where the second stream lives

**Not in `core/ChunkVtx.h`** — that is the terrain chunk's 32-byte layout and the subject path does not
use it. The subject path packs one buffer per attribute at offset 0 (`GltfStudio::PackVertices`,
`SubjectDraw::ShapeOf`), so a second uv set is **a sixth run and a new `VertexLayout` value**.

- [ ] **`board:1156`'s permutation argument applies before the value is added.** `VertexLayout` is already
  a packed product of independent booleans decoded by `CarriesUv` · `CarriesNormal` · `CarriesTangent`,
  and a fourth boolean takes 5 named values toward 8–10. **Adding one more enumerated combination is the
  shape that item is filed against** — say whether the layout becomes a flag set here or record why it
  stays an enumeration
- [ ] **`board:1177` owns the per-reference `texCoord` field and this task owns the stream**, which is the
  split already written into `board:1177`. Neither half-implements the other

## The invariant this inherits, and it NARROWS rather than disappears

**`board:1177` made a texture reference whose `texCoord` names set 1 on a one-set subject a NAMED
REFUSAL.** When this lands:

| subject | verdict |
|---|---|
| carries `TEXCOORD_1` | **resolves** |
| does **not** carry it | **stays a named refusal** |

**Never a fall-back to set 0.** The refusal is about the **subject's attributes**, never about the
engine's capability, and turning it into a fallback would make a missing attribute render as a plausible
picture — the same failure this task's own defect shape already is. **A test must hold the refusal after
the feature lands**, or the narrowing is an assertion.

**Done when** a subject's second uv set reaches the sampler for the references that name it, `MultiUVTest`
is in the tree with the two sets shown to differ by more than the bound, the refusal survives for subjects
without the attribute and is held by a test, and the layout's permutation cost is answered rather than
inherited.

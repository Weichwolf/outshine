Type: task
Parent: 0079
Area: gltf
Tags: khronos, oracle, instrument

**`KHR_texture_transform`, and the precedent for every data extension behind it**

`board:0079` row: **impact 15 of 146**, tier 1, and [MEASURED] **genuinely absent — `texture_transform`
appears in 0 files under `src/`.** It is **ratified**, so it is in scope under the owner's *glTF 2.0 with
extensions* ruling and `board:1172`'s status line.

**Parented to `board:0079` rather than `board:0078`, and the distinction is not a formality**: `0079` is
one line per **capability** and this delivers a capability row; `0078` orders **which assets in which
sequence** and is cited below for the case it supplies. A task under the asset matrix would be a task
about fetching a model.

## THIS IS THE FIRST EXTENSION IMPLEMENTED UNDER THE RULING, so its shape is the precedent

`board:0078` already records `KHR_materials_unlit` as the precedent for **lobe** extensions — *a declared
enumeration choosing among arms the renderer implements is not content shipping a program.* **This is the
precedent for DATA extensions**, and twenty-odd rows sit behind it. Three decisions travel with it:

- [ ] **THE DEFAULTS ARE THE IDENTITY, so absence and presence-with-defaults are ONE computation.**
  `offset [0,0]` · `rotation 0` · `scale [1,1]` compose to the identity, so a material without the
  extension carries the identity and **no branch, no second shader arm and no new pipeline permutation
  exists.** That is what keeps `board:1156`'s permutation count from multiplying once per data extension,
  and it is the property every later data extension must be checked against
- [ ] **The extension's data lives in the material row**, which is the only thing already bound per
  surface slot — the same argument `board:1138` recorded for the slot index, and a second per-slot binding
  would be a second thing to keep in step
- [ ] **The reader signals presence by writing values, never by a flag.** A `hasTransform` boolean is a
  field that can disagree with the numbers beside it

## What the extension defines, from the specification

| property | type | default | units |
|---|---|---|---|
| `offset` | array[2] | `[0.0, 0.0]` | texture dimension factors |
| `rotation` | number | `0.0` | **radians** |
| `scale` | array[2] | `[1.0, 1.0]` | unitless |
| `texCoord` | integer | *(none)* | index |

**Composition order: translation × rotation × scale.** **Rotation is counter-clockwise about the origin
on the UVs** — *"equivalent to a similar rotation of the image clockwise"*. **`texCoord` overrides the
`textureInfo`'s own value when supplied**, and **the extension applies PER TEXTURE REFERENCE — inside each
`textureInfo` — never per material.**

**THE ORDER IS COMPOSED ON THE HOST AND THE SHADER RECEIVES A MATRIX.** Translation × rotation × scale is
fixed at load, so the row carries a **2×3 uv matrix, six floats per texture reference**, and the fragment
does one multiply-add. **This makes *the wrong order* unspellable in the shader** — there is no sequence
there to get wrong — and puts the composition where a C++ unit test can hold it against the specification.
It also removes a per-fragment `sin`/`cos`. **Cost, stated: four references × six floats takes
`kSurfaceFloats` from 13 to 37.**

## The `texCoord` override is shared with row 3 and must not be half-implemented twice

**`texCoord` selects which UV set a texture reference reads, which is exactly `TEXCOORD_1`'s subject.**
`board:0079` row `TEXCOORD_1` carries an `UNSURE` on which of a material's textures may name a second set,
and **that question is answered here or there, once.** This task **owns the per-reference `texCoord`
field**; `TEXCOORD_1` owns **the second vertex stream and the layout that carries it.** A transform whose
`texCoord` names set 1 on a subject with one uv set is a **named refusal**, not a silent fallback to set 0.

## Which case proves it, and it is a new case rather than a new field alone

**Neither model is in the tree** — no `TextureTransformTest`, no `TextureTransformMultiTest` under
`test/render/` and neither fetched. Both are in the pinned index tagged `testing, extension`. **So this
task is a material field, a reader path, a shader term AND a corpus case**, which is a different size from
the row's tier-1 label and is stated here rather than discovered.

- [ ] **`TextureTransformTest` is the mechanism case** — one texture, the transform visible as a moved,
  rotated, scaled image
- [ ] **`TextureTransformMultiTest` is the one that catches the real defect and it is not optional.** **An
  engine that applies one transform to all of a material's textures passes the single-texture case and
  fails this one**, because the extension is per texture reference. A round that lands only the first case
  ships exactly that defect green

## What a wrong implementation looks like, so the test can fail

| defect | what the picture does | which case sees it |
|---|---|---|
| composition order reversed (S×R×T) | the offset scales with `scale` and shifts with `rotation` | `TextureTransformTest` |
| rotation sign flipped | the image turns the wrong way — **invisible at rotation 0 and at π**, so the case must declare a rotation that is neither | `TextureTransformTest` |
| applied per material, not per reference | one texture right, the others carrying its transform | **`TextureTransformMultiTest` only** |
| `texCoord` override ignored | the transform lands on the wrong uv set | a multi-UV subject |
| transform applied to the sampled result rather than the coordinate | a shifted *colour*, not a shifted *image* | any |

**And the wrap mode interacts, which is why one existing case is named here.** An offset or scale that
pushes uv outside `[0,1]` is decided by the sampler's wrap mode, so a transform case is also a wrap case.
**`texture/texture-settings-test` is already in the corpus and already failing** — with 77 identity
disagreements counted under `board:1155` — so **its state must be read before a transform case is scored
against the same sampler**, or two questions arrive as one number.

**Done when** a texture reference's transform reaches the sampler as a host-composed matrix, a material
without the extension computes the identity through the same path with no branch, `texCoord` overrides the
reference's set or refuses by name, both transform cases exist and the multi-texture one is inside the
picture bound, and the three precedent decisions above are stated where the next data extension will read
them.

## Comments

**CLOSES on four of five clauses. The fifth is SPLIT — half struck with its refutation, half re-homed.**

**The struck half is mine and it was a directive, not a guess.** This item said *an engine that applies
one transform to all of a material's textures passes the single-texture case and fails only
`TextureTransformMultiTest`.* **Measured against the file at the pin, that is false.** No material in
either transform asset carries **two different transforms on two references of one material**; each of the
multi test's 29 materials has exactly **one** transformed reference. **It separates *transformed on every
socket* from *transformed on base colour only*, and it does not separate per-reference from
per-material.** So the clause asked a case to prove something the asset does not contain — the
`board:1127` shape, and the correction is the same: **strike the claim, keep the requirement.**

**The half that stands is re-homed to `board:1180`**: the multi case is wanted, it exists upstream, and it
is blocked by rows that are not this one's — `TEXCOORD_1` 9 of its 27 cells, `KHR_materials_clearcoat` 18,
occlusion 3. **`Depends:` is the honest edge**, and it is written there rather than left as an unmet
clause here.

**The per-reference claim is now held by a unit test and by nothing in the corpus** —
`EveryTextureReferenceCarriesItsOwnTransform.cpp`, one material, five sockets, five distinct transforms,
17 claims failing under the per-material mutation. **That is real proof and it is not a render**, so
*pass the Khronos corpus* cannot reach it. Filed as `board:1179`.

**What the round proved beyond its acceptance, and it is the identity-default precedent working.** Every
pre-existing textured case is **bit-identical to baseline** — `simple-texture` 9.9012458e-06,
`texture-coordinate-test` 10.295625, `scifi-helmet` 15.457417, `normal-tangent` 229.33018. **That is the
predicted result of *absence and presence-with-defaults are one computation*, not a null one**: a branch
would have moved something.

**The new case is red at `picture_max_delta_code` 9.5930063 against 6.4354338, and its residual is
attributed rather than absorbed.** The first attribution — the mip chain — **refuted itself by
measurement**: enabling `kChainIsReadable` takes the metric **9.593 → 186.118**. What survives is the
sub-texel weight term **at a whole 2⁸ division rather than half of one**, `255·12.92/256 = 12.8695` codes,
with `texture-coordinate-test` sitting at `10.295625` by the same arithmetic. **The bound was not
widened**, and `board:1151` now has a **second** case pointing at one derivation — which is worth more to
that item than either case alone.

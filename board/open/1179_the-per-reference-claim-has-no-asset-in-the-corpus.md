Type: bug
Area: corpus
Tags: khronos, oracle, instrument, scope

**The per-reference claim has no asset in the corpus, and only a unit test holds it**

`KHR_texture_transform` applies **per texture reference**, inside each `textureInfo`. **No Khronos sample
model separates that from per-material.** Measured against the pin: neither `TextureTransformTest` nor
`TextureTransformMultiTest` carries **two different transforms on two references of one material** — each
of the multi test's 29 materials has exactly one transformed reference, and what it varies is *which
socket* is transformed, not *how many transforms one material holds*.

**So an engine that applies one transform to all of a material's textures passes every Khronos transform
asset.** The claim is held today by `EveryTextureReferenceCarriesItsOwnTransform.cpp` — one material, five
sockets, five distinct transforms, **17 claims failing under the per-material mutation.** That is real
proof and it is a **unit** test.

**Why that is a defect and not a preference.** Phase 1's sentence is *pass the Khronos test corpus*, and
the owner's rule is *every glTF 2.0 feature supported and **tested***. **A capability whose only render
proof is impossible from the corpus is a hole in what passing the corpus means** — the same shape as
`board:1146`, where a suite's verdict was quoted more broadly than its domain. **It is not an argument for
weakening either rule**; it is the measurement that says the corpus is not sufficient for one named claim,
and that has to be written down where the claim is counted.

**What a generated fixture would cost, because that is the alternative and nobody has priced it.**
`test/outshine/corpus/prep/fixtures.py` generates subjects through two families — declared shapes, and
`grown.SHAPES` where the engine itself is the producer. **Neither produces a material or an image today**:
every textured case in the tree gets its texture from a fetched asset. So the fixture this claim needs —
one mesh, one material, several sockets, distinct transforms, distinguishable images — requires
**image synthesis in the preparer**, which is a new capability there and not a new row in a table.

- [ ] **Price it before choosing it**: a fixture needs a deterministic image writer whose bytes are stable
  across hosts, because the oracle key covers the subject's bytes. A PNG written by a Python loop is
  reproducible; anything through a library is a dependency question
- [ ] **The alternative is to leave the claim with the unit test and SAY SO where the corpus count is
  published** — which is honest, cheap, and keeps *pass the Khronos corpus* from being read as *every
  claim is proven by a picture*
- [ ] **A generated fixture is not a substitute for an upstream asset** and must not become one by habit:
  it proves our reading of the specification against our own implementation, with no third party in the
  path. `board:1178` is the case where the third party was wrong, which is exactly when that matters

**Done when** the per-reference claim's evidence is stated where the corpus counts are published, and
either a generated fixture exists with its cost paid or the decision to leave it with the unit test is
recorded with its reason.

**The fixture capability is `board:1190`** and it is filed as one item because three items need it. **This one carries a line rather than a `Depends:`**: its second path — leave the claim with its unit test and say so where the corpus count is published — needs no fixture at all, and an edge would hold it out of *ready* for half of itself.

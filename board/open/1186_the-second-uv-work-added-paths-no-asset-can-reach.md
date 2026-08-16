Type: bug
Area: corpus
Tags: khronos, instrument
Depends: 1190

**The second-uv work added paths no asset can reach**

`board:1182` landed correctly and left two populations that nothing exercises. **Both are asset gaps and
both would close with one fixture**, which is why they are one item.

**Three of the eight vertex layouts are unreachable by any corpus asset.** `PositionNormalUvUv1` and
`PositionNormalUvUv1Tangent` compile, build pipelines and are held **only** by a `static_assert` and an
exhaustive switch. **A pipeline nothing draws with is `board:1174`'s *supported and unproven* column in a
new place** — and it is the more dangerous half of that column, because a layout that is never bound
cannot be wrong in a way any run would notice.

**The engine-side second-uv refusal has no standing test.** `SubjectDraw::SetMesh` refuses **per draw**;
the `gltf`-layer refusal is **per subject**. So a subject with **one part carrying `TEXCOORD_1` and
another not** is the only thing that reaches the draw-level branch, and no asset in the corpus is shaped
that way. Its refusing branch is exercised by nothing.

**And closing it is a harness decision rather than a code one**, which is why it has sat: a refusing
render case **renders nothing and is red by construction**, `test/unit/render/stages` links no device, and
`test/shader` links one file by a deliberate harness decision. **Three suites, and the branch falls
between all of them.**

- [ ] **One generated fixture reaches both**: a subject of two parts, one with `TEXCOORD_1` and one
  without, the first also carrying `TANGENT`. That is the mixed-part subject the draw-level refusal needs
  **and** the layout combination no Khronos asset provides
- [ ] **It requires the same capability `board:1179` priced** — `fixtures.py` generates no material and no
  image today — so the two items share a prerequisite and **should be dispatched together or not at all**,
  or the second pays a cost the first already paid
- [ ] **A refusal is proven by the refusal, not by a picture.** The case's acceptance is that the run
  **names the refusal and the part**, and a case whose subject is a refusal is green when it refuses —
  which is a verdict shape this suite does not have and must declare before the fixture exists
- [ ] **The unreachable layouts are counted where the layout count is published**, so *8 layouts, 48
  pipelines* is never read as *8 layouts exercised*

**Done when** every vertex layout the build creates a pipeline for is bound by something that runs, and
the draw-level second-uv refusal is reached by a test whose verdict is the refusal itself.

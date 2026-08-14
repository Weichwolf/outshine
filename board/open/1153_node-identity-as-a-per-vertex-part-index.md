Type: task
Parent: 1137
Area: render
Tags: instrument, oracle

**Node identity as a per-vertex part index**

`board:1138` delivered the material half of *what covers this pixel* and named why the node half is out of
reach through the same door: **`DrawList::Compile` merges primitives of different nodes into one
`DrawBatch` when they share a material**, so the finest identity a per-slot uniform can carry is the
surface. The oracle's `objectIndex` is therefore read for discrimination and adjudicates nothing.

**The route is a per-vertex part index and it costs no batching.** The identity travels with the vertex
rather than with the draw, so two nodes sharing a material stay in one batch and still say which of them a
fragment came from — a sixth vertex stream, flat-interpolated, at 4 bytes a vertex. **The alternative —
splitting batches by node — would pay the instrument's cost in the shipping frame, which is the trade this
engine refuses everywhere else.**

- [ ] **Flat-interpolated, and stated as flat rather than assumed.** Barycentric interpolation of three
  equal integers is a weighted sum that need not return them; the emitted-radiance stream in this same
  vertex layout already carries `[[flat]]` for exactly that reason, and the same sentence applies here
- [ ] **The index is the part's position in the subject's own `Parts()`**, so it is the number the reader
  already has, and it is written one higher than the part so that **zero means nothing drew here** — the
  convention `SceneSurfaceIdentity` and the shading-normal attachment both already use
- [ ] **It rides in the identity attachment that exists**, in a channel that is currently a declared zero.
  `Rgba32Float` carries four channels and one is used; a second target for a second index would be a
  second thing to keep in step for no gain
- [ ] **The Blender correspondence is derived and checked exactly as the material one was.** `pass_index`
  is assigned in name order over `bpy.data.objects` including the factory file's own, so index *n* is not
  object *n*; each name resolves to **exactly one** glTF node or it is a refusal
- [ ] **It unlocks the nine per-node emission cases that refuse today** — a case whose emitted radiance is
  declared per node cannot be adjudicated while every node wearing one material is one identity

**What must be measured before it is believed**: that a case whose nodes share a material reports more
than one distinct part index over its covered region. The vacuity check `board:1138` built is the
instrument, and **18 of 35 cases already refuse on it** — a part index that reported one value everywhere
would join them and say nothing.

**Done when** a pixel can be asked which node drew it, the answer is checked against the oracle's
`objectIndex` through a derived correspondence, and a case whose nodes share a material adjudicates rather
than refuses.

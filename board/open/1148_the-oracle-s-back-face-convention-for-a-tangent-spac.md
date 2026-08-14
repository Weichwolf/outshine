Type: feature
Area: render
Tags: oracle, khronos, instrument

**The oracle's back-face convention for a tangent-space normal map is measured and declared**

Nothing in this tree states what Cycles does at a back-facing fragment carrying a tangent-space normal
map, and until it does, **no case can score one**. `board:1147` can produce the population with our own
render; it cannot say whether a disagreement there is our defect or a convention difference, which is the
whole reason a reference exists.

**Three questions, and each is a separate answer that a single render can confuse.**

- [ ] **Does the shading normal flip at all on a back face**, or does Cycles shade with the normal the
  geometry supplies and let the BSDF handle the hemisphere?
- [ ] **Does the tangent frame flip with it** — the tangent, the bitangent, or both — so that the map's
  green channel runs the same way across the surface as it does from the front?
- [ ] **What does the importer's material graph do**, which is a different question again: the oracle's
  material is built by the pinned glTF importer, not by us, and a Normal Map node's behaviour is the
  importer's choice as much as Cycles'.

**The instrument exists and needs no new machinery.** The oracle already publishes its own `Normal` pass,
and the three-leg comparison built for `board:1122` and `board:1126` already puts ours, Cycles' and the
**file's declared** normal side by side. So the measurement is: one small subject with a deliberately
non-neutral normal map, rendered from the front and from behind, with the three legs read at the same
texels. **The file's leg is what adjudicates**, exactly as it does for the front face — it is authored
outside this tree and it is the frame the perturbation is relative to.

**THE ANSWER IS NOT LOOKED UP FROM THE SAMPLE RENDERER, AND THAT IS THE POINT.** Khronos's
`material_info.glsl` says what a **glTF renderer** must do, and `board:1127`'s repair is held to it
correctly. Cycles is a path tracer with its own convention and its role here is *is this the right image*.
**So a disagreement is a reduction question on `board:0087`'s ladder before it is a defect of ours** — fix
the engine · reduce the oracle · patch the asset · disqualify — and this item exists so that the ladder
can be climbed with a number instead of an assumption.

**It is a property of the PIN and it is re-measured when the pin moves.** The answer belongs beside the
oracle's other declared limitations, with the Blender build and the importer it was measured on written
next to it. The preparer digest already forces a re-render when either moves, so the statement cannot
quietly go stale — but nothing forces a re-*reading* of it, and that is why the measurement must be a test
rather than a note.

**What would make this cheap to get wrong**: measuring it on `materials/normal-tangent-mirror`, whose
front face is already in dispute (`board:1126`) and whose whole subject is a mirrored tangent. A back-face
convention read off that asset would be three effects summed. **The subject must be plain**: one
double-sided facet, one supplied tangent, one map with a strong and asymmetric perturbation, and nothing
mirrored anywhere.

**Done when** the convention is a measured statement with its numbers and its pin, published where the
oracle's limitations are declared, and `board:1147` can route a back-facing disagreement to *ours*,
*theirs*, or *the ladder*.

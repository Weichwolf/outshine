Type: bug
Area: render
Tags: instrument

**Roughness and metalness are lost when a material has a metal-rough image and no normal map**

`metalRoughMap` is sampled in exactly one place — `mappedShade` — and the mapped arm is selected only when
`mapped = lit && textured && HasTangent() && Surfaces[slot].Normal.Rgba`. The reader generates a tangent
basis only where *the material actually samples a normal map*, which is the format's own condition and is
right. **So a lit, textured part whose material declares a `metallicRoughnessTexture` and no
`normalTexture` takes the plain lit-textured arm, and both roughness and metalness fall back to the
material's factors — which default to 1.0 and 1.0 when the file states none.**

At roughness 1 the GGX distribution is the constant `1/pi`, so the lobe stops depending on the normal; at
metalness 1 the diffuse colour is `albedo * (1 - 1) = 0`. **The surface renders as a flat, diffuse-free
metal sheen**, which is the exact consequence `mappedShade`'s own comment names as the reason for sampling
the image — stated on the arm that does sample it, and unguarded on the arm that does not.

**glTF ties the two textures together nowhere.** `metallicRoughnessTexture` and `normalTexture` are
independent material properties; a material may declare either without the other. The engine's coupling is
an artefact of which shader arm owns the sampler, not a rule of the format.

**The domain, and it is why this is filed rather than repaired inside another item.** *No case in the
corpus exercises it today* — every material in the tree that carries a metal-rough image also carries a
normal map (`normal-tangent`, `normal-tangent-mirror`, `boom-box`, `corset`, `lantern`, `water-bottle`,
`a-beautiful-game`, `scifi-helmet`), so the arm selection is correct on every asset present and the defect
is **latent rather than measured**. That is the whole reason it needs a work item: it is invisible to the
suite and will appear as a shading disagreement nobody can attribute the first time an asset without a
normal map arrives. `board:0078`'s rung 17 metal-rough sweep is the obvious candidate and is **not
verified here**.

**What would be right instead.** The metal-rough tap belongs to *textured and lit*, which is the condition
that makes it readable, and not to *has a tangent frame*, which is the condition the normal map needs.
Either the lit-textured arm samples it, or the arm selection is a decision made from the **surface's own
declared slots** rather than from the vertex layout — the second is the shape that makes the coupling
unspellable rather than merely fixed, and it is the same observation `board:0112` makes about the
pipeline key being `(VertexLayout, SurfaceState)`.

**Done when** a lit textured part reads the metal-rough image its material declares whatever the tangent
frame says, and a case exists that would have failed before it.

---

Closed -- the metal-rough tap moved to the condition that makes it readable: the three
lit-textured arms (opaque, masked, blended) sample SUBJECT_METALROUGH_TAP and shade with
surface.metalness * orm.b and surface.roughness * orm.g; an absent image samples the
residency's neutral white, so factor-only materials are untouched, and the mapped arm keeps
its Toksvig term. Proven on the device in
render/outshine/shader/AMetalRoughImageReadsWithoutATangentFrame: a tangentless quad whose
image says dielectric rough 0.5 against factors saying metal rough 1 renders 62% apart from
its factors-only twin -- the arm that ignored the sampler rendered them identically
(negative control: the pre-fix shader reverted fails all three arms of this test).
Residue for the board: the lit TRANSMISSIVE arm (fsLitTransmissive) still shades untextured
even when the material declares images -- the same class, one arm over.

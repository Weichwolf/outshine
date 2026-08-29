Type: bug
State: active
Area: test, door
Tags: measured

# A corpus case drives the door

**Benchmark** — Unreal: its automation tests drive the ENGINE (`UWorld`, `FSceneView`,
`FAutomationTestBase`) and never a render module's internals, which is why a renderer rewrite does
not touch them. RAGE: the same, one layer down -- a test harness stands on the game layer's public
surface. **They agree**, so the matter is closed: a conformance case exercises what a client can
reach, or it is not measuring the product.

MEASURED:

    test/harness/shared/render/Parity.cpp        2430 lines
    the whole render harness                     6629 lines
    apps/demo/src/main.cpp (a real client)        195 lines

    #include in Parity.cpp                         41
    of those, from include/ -- the door             0

Not one. The tree's ONLY oracle -- the corpus a case fails because the code is wrong -- reaches
`Render::SubjectProxy`, `Render::SceneRenderer`, `Gltf::Subject`, `Gltf::Document`,
`Gltf::ResolveSurfaceTable`, `Render::SurfaceTable` and thirty-five more internal headers, and
never `Engine`, `Scene`, `View`, `Camera` or `Renderer`.

**THIS IS THE FIRST TRAP IN CLAUDE.md's OWN TABLE**: *a gate blind to a path -- vendor cases green
while engine cases are red, because the harness bypasses the engine's own submission*. It was
written down and then not applied to the one harness where it costs most.

THREE CONSEQUENCES, ALL MEASURED TODAY:

- The corpus went RED-BY-BUILD on two commits in a row (d768f13e, 1a1a68b6) that changed nothing a
  client can see -- a type alias and a file move. Neither would have touched a harness that spoke
  the door. Both were committed unnoticed because the runs that followed them were `harness/claims`
  and `outshine/places`, and neither of those compiles this file.
- The line count is the door's own instrument (CLAUDE.md: *a client is almost no code and its LINE
  COUNT measures the door*). 2430 lines to load a glTF, place a camera and render one picture says
  the door cannot do it, and the harness has been carrying the difference.
- The parity oracle therefore states that the RENDER INTERNALS agree with Khronos, not that
  outshine does. Whether the engine's own submission path draws these files at all is untested.

## WHAT THE HARNESS REACHES FOR, AND WHAT THE DOOR ANSWERS

Measured by listing every internal call the file makes. This is the door's shopping list, and each
row is a benchmark question rather than a wish.

| the harness reaches | the door today | the name it should wear |
|---|---|---|
| `ReadSceneLinear` | nothing -- `readPixels` is 8-bit sRGB | Filament's `readPixels` takes a pixel FORMAT, so a float one is the same verb |
| `ReadDepth` | nothing | the same verb, naming the depth attachment |
| `ReadShadingNormal` · `ReadSurfaceIdentity` · `ReadSceneVelocity` | nothing | Unreal's buffer-visualisation targets, named |
| `SubjectDrawCount` · `SubjectBatchCount` | nothing | Unreal keeps them in `FSceneRenderer`'s stats; Filament has none. **Ours**, and the reason is that board:1943's whole claim is a draw count |
| camera: orthographic, near, far | `Camera{Placed, Stands, FovDeg}` | Filament's `Camera::setProjection(Projection, ...)` carries kind, near and far |
| `studio.Around` · `IndirectLight` | `Lighting::IndirectLight` -- **already there** | Filament `Scene::setIndirectLight` |
| `studio.Lit` · `Lights` | `Lighting::Key` -- **already there** | Filament `Scene::addEntity` |
| `studio.Emits` | `Material::Emissive` -- **already there** | |
| `studio.Posed` | a scenario's animation -- **already there** | |
| `renderer.Init` · `DeviceUsable` · `RenderFrame` · `SettleFrames` | `Engine::renderer()`, `render`, `settleFrames` -- **already there** | |

Half of it the door already answers and the harness reaches past anyway, which is its own finding:
the file was written against the internals and never revisited. The other half is real and is what
this item buys.

## WHERE THE CONVERSION STANDS, MEASURED ON ONE CASE

Driving `AlphaBlendModeTest` alone -- seconds rather than the suite's 23 minutes -- took the
converted harness from every case failing to one metric:

    disagreement_p99_px          104.74 px  ->  0.000 px   (bound 0.005)   the GEOMETRY is exact
    plan_passes                       3     ->      2      when the surface is not declared
    picture_p99_delta_code           177     ->    177     (bound 6.44)    the COLOUR is not

THE COLOUR IS CARRIED BY PER-PART EMITTED RADIANCE AND THE DOOR CANNOT SAY IT. The old harness
wrote it straight into the proxy -- `studio.Emits(part, rgb)` -- one radiance per PART. A
`SurfaceOverride` is keyed by the file's MATERIAL name, so two parts sharing a material cannot
differ, and this case's five quads share one. Measured three ways:

    indirect light set to 1.0 everywhere      177 -> 52    it reaches, and it is not the carrier
    indirect light as the old path set it     177 -> 219   the old path set NONE for this case
    per-node override attempted                            std::length_error, see below

Unreal overrides a material per COMPONENT SLOT and Filament hands out a `MaterialInstance` per
PRIMITIVE. **They agree** that the key is the part, not the material. So `SurfaceOverride` needs a
NODE key beside its material key -- and the attempt crashed, because giving one part its own slot
means growing `SurfaceTable` under a proxy that is already standing on it. The table has to support
a per-part slot properly rather than have one appended to it.

**THE PER-PART SLOT WORKS AND IT IS NOT THE WHOLE ANSWER.** Written and measured rather than
guessed: a `Node` key on `SurfaceOverride`, and a slot split per overridden part in the surface
table. With the emitted radiance said per node the colour goes

    picture_p99_delta_code   177 codes  ->  50 codes   (bound 6.44)

so the mechanism is right and something else carries the rest. `SubjectProxy::Emits(part, rgb)` and
`Material::Emission` are not the same quantity -- one is a per-part radiance the shader adds, the
other a material's emissive factor -- and which of the two the oracle was rendered with is the next
measurement rather than a guess.

**TWO CRASHES PAID FOR ONE RULE.** Splitting a slot per part means pushing into `SurfaceTable`, and
a `SubjectMaterial` holds RAW POINTERS into `Decoded` -- `Colour.Rgba` is
`Decoded[slot].Colour.Rgba.data()`. Growing that vector leaves every slot in the table pointing at
freed memory, which is a `std::length_error` several frames away from its cause. Reserving the
worst case first is the only version that is safe rather than lucky. The first attempt also called
`Reshape()` inside the same loop, re-forming the shape under the proxy that points into it.

Neither the `Node` key nor the slot split is in the tree, and the reason is this item's own rule: a
door field nothing reads is the defect. They land in the round that closes the colour, together
with the conversion that reads them.

**AND ONE MORE HYPOTHESIS DIED.** `Live` never calls `SubjectProxy::Emits`, so the per-part emitted
run is always zero in the engine's own path -- a capability no declaration reaches. The obvious
reading was that `Material::Emission` should fill it, making the two quantities one. Measured:

    emission per node, run left alone     50 codes
    emission per node, run filled from it 177 codes

Filling it made the picture WORSE, back to the value it has with no override at all. So the run and
the material row are not the same quantity said twice, and which one the oracle was rendered with
is still the open question -- but "they are one thing" is now a dead answer rather than a plausible
one, and that is worth more than the guess was.

**AND A THIRD READING DIED THE SAME WAY.** The case is flat-shaded, so the door's word looked like
`Material::Unlit` with the radiance as the base colour. Measured:

    emission per node                      50 codes
    unlit + base colour per node          177 codes -- the value with NO override at all

`Unlit` and `BaseColour` change nothing here, and the reason is visible in the case: its materials
carry a baseColorTexture, and a TEXTURE beats the factor. So the flat radiance can only arrive as
`Emission`, which is where the 50 comes from -- and what closes the last 43 is how the oracle
combines that emission with the texture it is standing on. That is the next measurement, and it is
a question about the SHADER rather than about the door.

WHAT THE THREE DEAD READINGS ARE WORTH. They cost one session and they remove three of the four
ways this could have been wrong: the indirect light reaches and is not the carrier; the per-part
run and the material row are not one quantity; and an unlit base colour cannot carry a textured
surface. What is left is one question with one place to look.

**AND THE SHADER ANSWERS IT.** The two quantities feed two different VARIANTS:

    src/render/shaders/subject.msl:36        o.col = float4(in.emitted * in.colour.rgb, 1.0)
    src/render/shaders/subjectLitTextured.msl:3   surface.emissive * SUBJECT_EMISSIVE_TAP(uv).rgb

The per-part run drives the FLAT variant -- emitted times the vertex colour, no texture, no light.
The material's emissive drives the textured ones, modulated by an emissive MAP. They were never
one quantity and could not be: one is what a surface IS, the other is what a surface ADDS.

So the last 43 codes are a question about which VARIANT this case's draws take, and a case whose
appearance is `factor x ambient` with no lighting is asking for the flat one. The door's way to say
that is the layout a draw is given, which `LayoutOf` decides from what the part carries and what
its surface reads -- and no client can state it today. That is the last door word this conversion
needs, and unlike the four before it, it is not a field that was already there and unread.

**AND THE FIFTH READING DIED TOO, WHICH IS WHERE THIS STOPS BEING GUESSWORK.** `Unlit` IS read --
`Lit()` turns a draw flat when its surface carries it -- so the chain looked closed: unlit picks
the flat variant, the flat variant reads the per-part run, and `Live` fills that run from the
material's emission for unlit surfaces only. Measured:

    emission per node, no unlit               50 codes
    emission per node PLUS unlit             177 codes

And the picture refuses the story outright: `Lit()` also requires lights or an indirect radiance,
and this case declares NEITHER, so the flat variant was already being chosen before `Unlit` was
touched -- yet the picture shows the file's TEXTURE, which `subject.msl:36` does not read at all.
So the draws are not taking the variant the code says they should.

**THE NEXT MEASUREMENT IS WHICH VARIANT EACH DRAW ACTUALLY TAKES**, read out of the compiled draw
list rather than reasoned from `LayoutOf`. Five readings of the colour are now closed and every one
of them was a story about WHAT to say; this is the first about what the engine DOES, and it is the
only kind left that can be true.

**THE INSTRUMENT NOW EXISTS AND IT ANSWERED ON THE FIRST READING.** `SubjectBatchesTaking(layout)`
counts the draws that took each vertex layout, and `Engine::render` publishes one row per layout
the frame actually used. On this case:

    draws taking vertex layout 4 = 8      PositionNormalUv
    draws taking vertex layout 6 = 1      PositionNormalUvTangent

Every draw takes a LIT, TEXTURED variant. Not one takes the flat one. So the whole line of
reasoning about `Unlit` and the emitted run was about a variant this case never uses -- and it took
a measurement to see, because `LayoutOf`'s inputs are three predicates deep.

**AND THAT IS ONLY POSSIBLE IF THE PROXY GATHERS LIGHT.** `Lit()` needs `Gathers(proxy)`, which
needs a punctual light or a non-zero indirect radiance. The case declares neither, and the file
carries none -- measured, `LAMPS file=0`. So something in the engine's own path lights this subject
where the old harness left it dark, and THAT is the colour difference. Finding what is the next
round's first line, and it is now a one-line read rather than an argument.

**THE VARIANT IS NOW RIGHT AND THE MECHANISM IS FULLY UNDERSTOOD.** Three things had to hold at
once and had only ever been tried one at a time:

  1. no ambient unless the case is shaded by lights -- otherwise `Gathers` is true, `Lit` is true,
     and every draw takes a LIT variant. Measured: 8 draws on layout 4 and one on layout 6.
  2. the emitted radiance said per NODE, which needs the node key and the per-part slot.
  3. `Live` filling the per-part run from `Material::Emission`, because nothing ever filled it.

With all three: **9 draws on layout 1** -- `PositionUv`, the flat textured variant, which is what
the oracle rendered. `subject.msl:76` is exact about what it then computes:

    o.col = float4(in.emitted * SUBJECT_COLOUR_TAP(...).rgb * in.colour.rgb, 1.0)

so the per-part run is a TINT the texture is multiplied by, not a radiance added to it -- and
`ResolveEmission`'s `baseColour x ambient` is exactly such a tint. The mechanism is closed.

The value is not: the picture reads 219 codes where the tint should make it match. That is now a
question about ONE number reaching ONE uniform, with the variant, the shader line and the quantity
all named -- which is a different kind of question from the five that died before it.

That is the next round's work, and it is the last thing between this conversion and the tree.

- [ ] A Khronos case reads a file, places a camera, renders, and compares -- through `include/`
      alone, and the driving part of it fits on a screen
- [ ] The scoring stays as long as it needs to be: an EXR oracle, a p99 delta and an acceptance
      class are the corpus's own work and are not the door's business
- [ ] `Parity.cpp` names no header outside `include/`, and a claim holds that count at zero
      proof: the negative control is one internal include, which turns the claim RED

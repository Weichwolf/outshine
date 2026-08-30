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

**AND THE PICTURE I WAS LOOKING AT IS NOT THE PICTURE BEING SCORED.** Run with the OLD harness and
clean objects, this case reads `picture_p99_delta_code = 1` and passes all 31 checks -- and the
`1-outshine.png` it writes beside itself is PURE MAGENTA. The metric compares the LINEAR readback
against the EXR oracle; the PNG comes from somewhere else and, on this path, from nothing at all.

So every visual reading in this item -- "too dark", "the framing matches", "the texture is there"
-- was of an artefact the score does not use. The NUMBERS stand, because they came from the metric.
The pictures were a second opinion nobody had checked, which is CLAUDE.md's own trap: *ask what the
measure cannot see before trusting the number it produced*, applied to the picture rather than to
the count.

**A CASE THAT WRITES A PICTURE MUST WRITE THE ONE IT WAS SCORED ON**, or the instruction to LOOK at
it is worse than no instruction: it invites a confident reading of the wrong thing. That is its own
finding and it is filed as board:2040.

**THE RATIO, MEASURED IN THE BUFFER THE METRIC ACTUALLY READS.** Mean red over the whole frame:

    the old path, flat        0.023480137     picture_p99_delta_code = 1 code, PASS
    the door path, lit        0.000848601     picture_p99_delta_code = 177 codes

**27.7x darker**, and the same run says why in the same breath: 8 draws on layout 4 and one on
layout 6, which are LIT variants, where the old path draws flat. A Lambertian shade divides its
irradiance by pi and spreads it over a cosine; a flat tint does not. That is the whole of the
difference, and it is now a number rather than an impression -- the impressions were all of `Rgba`,
which nothing scores (board:2040).

With the three-part fix -- no ambient unless shaded, emission per node, the run filled -- the
layouts go flat (9 on layout 1) and the metric reads 219, so the flat path overshoots in the other
direction. The next round measures the same mean under THAT combination: one number, one buffer,
and the answer is a factor rather than a hypothesis.

**ONE CASE CLOSED COMPLETELY, AND THE CORPUS SAID NO.** With all three parts held together --
no ambient unless shaded, emission per node, the run filled -- plus the surface declared only when
there is a window to present into, `AlphaBlendModeTest` reads

    picture_p99_delta_code   1 code    (bound 6.44)     PASS
    plan_passes              2 passes  (bound 2)        PASS
    disagreement_p99_px      0.000 px  (bound 0.005)    PASS
    31 checks, 0 failures

and the whole corpus then reads **78 PASS, 366 FAIL** where the old harness reads 441 of 444.

That is over-fitting, stated plainly: three coupled decisions were tuned against ONE case and the
other 443 disagree. 78 is up from the 3 the first conversion managed, so the direction is right and
the calibration is not -- and a fix that is right for one case and wrong for four hundred is not a
smaller version of the answer, it is a different one.

**WHAT THE NEXT ROUND MUST DO DIFFERENTLY.** Every reading in this item was taken on
`AlphaBlendModeTest`, which is flat-shaded, textured, and takes its colour from the file. A case
shaded by LIGHTS, one taking `ColourFrom::Row`, and one with no texture would each have refused a
different one of the three decisions -- and running four cases costs four seconds. The instrument
that made one case cheap was built in this item; using it on ONE case was the mistake, not the
instrument.

## THE SAMPLE THE NEXT ROUND RUNS, AND WHY IT IS FIVE

The corpus has five KINDS of case, counted from the manifests:

    emission-per-material        107   AlphaBlendModeTest
    emission-by-material-index    13   MeshoptCubeTest
    emission                      10   TextureCoordinateTest
    metal-rough                    9   BoomBox          -- the LIT ones
    diffuse                        6   BoxInterleaved

One representative of each costs about twenty seconds together. Measured against the three-part fix
tuned on the first of them:

    AlphaBlendModeTest      31 checks, 0 failures
    TextureCoordinateTest   31 checks, 0 failures
    BoomBox                 30 checks, 1 -- picture_p99_delta_code 29 codes, layout 6, LIT
    BoxInterleaved          27 checks, 1 -- picture_p99_delta_code 57 codes, layout 0, mean 0.000
    MeshoptCubeTest         33 checks, 2 -- velocity, and the animated grid

**BoxInterleaved is BLACK**: mean red 0.000 over the whole frame, because a flat draw's emitted run
is never filled and `subject.msl` draws `emitted * colour`. That is an engine defect the corpus was
hiding, not a harness one -- the old harness filled the run itself and so never asked the engine to.

The general rule that follows -- a flat draw emits its EMISSION, or its BASE COLOUR under the
scene's ambient -- was written and measured, and it does not close it either: AlphaBlendModeTest
regresses to 1 failure and the other four stand where they were. So the rule is not yet right, and
the next round tunes it against ALL FIVE rather than against one, which is the whole lesson of this
one.

**THE FACTOR OF 1.674 WAS NOT A COVERAGE AND THE ANCHOR WAS THE WRONG NUMBER.** Written here was

    BoomBox         lit      0.003953406  ->  0.002363626     ratio 1.673
    BoxInterleaved  flat     0.004304506  ->  0.002571654     ratio 1.674

read as a scale applied to the whole frame, and the derivation that turned it into a COVERAGE
divided each mean by BoxInterleaved's emitted radiance, 0.0407009, on the stated ground that "the
run written into the frame is IDENTICAL on both paths". **That ground was false, and it is what
made a shade difference look like a size difference.** Measured:

    coverage_fraction_outshine   0.10575955
    coverage_fraction_oracle     0.10575846      five digits -- the geometry was never in question

0.004304506 is the value that agrees with the oracle at a `picture_p99_delta_code` of ZERO.
0.002571654 is the LIT path over a surface whose oracle is a Diffuse BSDF, and it decomposes
exactly: the reading was (0.04326, 0.01279, 0.01279) against an oracle of (0.04070, 0, 0), so
F = 0.04 + 0.96*(1-nv)^5 = 0.2514 and diffuse = 0.8*(1-0.2514) = 0.5989 -- both measured channels
to four digits, and the two grey channels are `subjectLit.msl`'s ambient specular on a material
that has none. So the number this item told the next round to VERIFY AN ASSEMBLY AGAINST was the
broken one, and a round that reached it would have called the defect the fix.

**The lesson is the ground, not the arithmetic.** A ratio between two quantities is only a scale if
what is being scaled is the same thing on both sides; here it was not, and nothing in the reading
said so. `coverage_fraction_outshine` was already being printed and answers the question directly.

## WHERE IT STANDS, and what the next round does

The runner in the tree stands, aims, draws and scores a corpus case through `include/` alone.
`Drives` is 125 lines, compiles against `-Iinclude` and nothing else, and a case drives in about
twenty. Measured over the 151 prepared Khronos cases, converted runner against the standing one:

    converted   150 green, 1 red -- ABeautifulGame
    standing    150 green, 1 red -- SpecularTest

Case for case, and the two reds are DIFFERENT cases: SpecularTest agrees through the door and did
not before.

**THE CAUSES FOUND, in the order they were found, because each was a door defect and not a case's:**

| what was wrong | what it cost | what it is now |
|---|---|---|
| an override could only be keyed by NAME, and a file may carry none | a declared row reached nothing | `SurfaceOverride::Part`, keyed by ordinal as Filament and Unreal both key it |
| an override left the asset's maps bound under a row that replaced them | a flat emission drawn as `emission x colourTap` | replacing is the default; `KeepsMaps` is Unreal's other verb |
| the plan asked `file.Materials()` whether the scene carries glass | 21 opaque cases ran two transmissive passes | it asks the resolved slots, which is what draws |
| a headless frame still ran a present pass | three passes where two suffice | `Surface` is not requested; the readback takes `FrameTex` |
| every part split its slot, leaving the original worn by nobody | 32 cases refused over a transmissive orphan | only a SHARED slot splits |
| a bounds sweep posed every frame and RECORDED each | 346.7 px of velocity on a still frame 0 | `Posed::Measures` poses without recording |
| the residency guessed a ring depth of 3 and a width of one pose | a 128-byte hand refused mid-sequence | one buffer, grown, and `cycle` on the first map of a frame |
| the door spelled a camera's roll as an ANGLE with no stated convention | a rolled facet sharing 48% of its pixels | `UpM`, which is what `Camera::lookAt` takes |
| the key light's DIRECTION was never handed over | 19 cases lit from elevation 0, bearing 0 | translated to elevation and bearing at the boundary |

**THE CORPUS RUNNER ENTERS THROUGH THE DOOR AND NOTHING ELSE.** `test/run.sh` hands
`harness/khronos/glTF` exactly this:

    -Iinclude -Itest/harness/shared -Itest/harness/shared/render -Itest/harness/shared/corpus

Not one path from `src/`. A rename anywhere under `src/` cannot reach a Khronos case, which is
what the goal asked for and why: a vendor case must break when BEHAVIOUR changes and never when a
file moves. That line is the guard, so widening it is the finding rather than the fix.

**AND A CASE DRIVES IN A SCREEN.** 37 lines against `-Iinclude` alone -- declare the asset, the
frame, the stages, the outputs and a view; stand it; bracket a frame; read the picture back.

**WHAT THE DOOR GAINED, and every one of them because a client could not do without it:**

| verb or type | what could not be said before | who spells it that way |
|---|---|---|
| `Loaded` | what is IN a file, and what it looks like at second t | `gltfio::AssetLoader` + `Animator` |
| `Loaded::camera` / `cameras` | the camera an asset ships | glTF's own `cameras` |
| `Loaded::frames` | the camera that FRAMES an asset that ships none | `Camera.viewBoundingSphere` |
| `Engine::camera` | the camera the FRAME is aimed with | `View::getCamera()` |
| `Camera::view` / `projection` / `clipMatrix` | where a point lands on the frame | `Camera::getViewMatrix()` |
| `Camera::kNearestM` | the near plane a frame stands on when none is declared | -- |
| `Texture.h`: `SurfaceMap` · `Sampler` · `ImageView` | that a surface WEARS a picture | `MaterialInstance::setParameter` |
| `Material`'s seven maps · `Geometry::addImage` / `images` / `imageAt` | which picture, which uv set, which sampler | glTF's material and texture tables |
| `Geometry::setSurface` | restating a surface rather than rebuilding the table | -- |
| `Logging.h` + `Engine::logsTo` | receiving the engine's running account | -- |
| `SurfaceState.h` · `UvTransform.h` | door vocabulary that stood BEHIND the door | -- |
| `Json.h` | a client reading its own declarations | -- |

**THE ONE THAT IS A JUDGEMENT AND NOT A TIDY-UP IS `Json.h`**, and it is written down so it can be
overruled. Three roads led out of the last `src/` path: leave it and fail the goal's own test;
write a second reader, which this page calls the worst outcome available; or publish the one that
exists. I took the third. What moved is 84 lines of READ-ONLY value tree over the standard library
-- no writer, nothing that knows a triangle from a texel -- and the six places in `src/` that
parse glTF, ground materials, vegetation and tree species keep using the same one, so there is
still exactly ONE JSON reader in this tree. It does not pass the door's admission test as written:
a client can use this engine without ever reading JSON. **If that is the wrong call the fix is one
`git mv`.**

The PNG went to `IMG_SavePNG` on the way, which is what this tree's own rule says to do where SDL3
supplies the function.

**AND THE PREPARED CORPUS IS GONE.** The system's temp cleaner emptied
`/var/folders/.../outshine-prepared` mid-session: every case reports UNPREPARED, the inputs
deleted, only this session's outputs left. The 150/151 above is the last reading taken against
real inputs. **A run measures nothing until `test/harness/shared/corpus/prepare.py` has fetched
them again**, and a round that starts by reading a number off a run will read zeros.


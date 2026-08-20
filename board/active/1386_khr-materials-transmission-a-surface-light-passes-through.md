Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr materials transmission a surface light passes through**

A transmission factor and an optional texture, for glass and for anything a viewer sees THROUGH rather
than a surface a viewer sees. **The most common non-opaque surface in a built world is a window**, so
this is not an enhancement.

**Two models at the pin require it**; thirty-three use it -- the widest gap in the honoured list.

- [x] What a transmissive surface does to the frame graph is decided BEFORE the lobe: it needs what is
  behind it, and that is a pass question and not a material one. The compiled plan answers it with a
  `SceneTransmissive` target the transmissive draws read, and `plan_passes` reads **4** against **2**
  for an opaque case -- a number the case declares its class for rather than a constant
- [x] It composes with `KHR_materials_ior`, and the composition is the OPPOSITE of what a reader would
  guess. **The extension treats a surface as infinitely thin and states microfacet-level refraction
  rather than MACROSCOPIC refraction** -- light passes straight through, blurred only by roughness -- so
  an index does NOT bend a transmitted ray here. Blender's importer wires a glTF `ior` into the
  Principled BSDF's own, where transmission then does bend it, and the first render of the new case
  showed the body split into three refracted faces on the oracle's side and one straight silhouette on
  ours. **The case declares `ior` 1.0 for that reason**, and what it decides is transmission rather than
  which renderer refracts
- [ ] A stall is worse than a wrong pixel: whatever this costs is priced by the frame suite

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_transmission>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: data** (see the parent's table).

## Measured, so the round that builds this does not start by counting

**It is the largest coherent red class in the corpus and it is worth six cases, not two.**

Four cases score today and disagree about COVERAGE because we draw glass opaque and the oracle sees
through it -- [MEASURED] by IoU against the oracle's silhouette:

| case | IoU | worst disagreement |
|---|---|---|
| `GlassVaseFlowers` | **0.70129** | 13.84 px |
| `DiffuseTransmissionTest` | 0.86663 | 6.94 px |
| `CompareTransmission` | 0.87801 | 25.46 px |
| `TransmissionOrderTest` | 0.99814 | 2.52 px |

and two more refuse at the reader because they REQUIRE the extension: `CommercialRefrigerator` and
`PotOfCoalsAnimationPointer`. **Thirty-three of the 148 models use it** -- the widest gap in the
honoured list by a wide margin.

## What is NOT wrong today, checked before it was assumed

**The reader does not read the extension at all.** `Material::Transmission` exists as a field and
nothing fills it from glTF; `Emit.cpp` refuses to WRITE a transmissive material without the extension.
So this is honestly *not built* rather than half-built, and there is no silent wrong picture to undo
first -- which is the state `kHonouredExtensions`' own rule exists to keep.

## Why this one is not a material row and cannot be done the way the others were

`KHR_texture_transform` and `KHR_materials_specular` are DATA extensions: numbers composed at the
reader that a fragment already in flight can use. **A transmissive surface needs what is behind it**,
which no fragment has. In a rasteriser that is a PASS -- the opaque scene resolved into something the
transmissive draw can read -- so this lands in the render plan of `CLAUDE.md`'s stage catalogue and not
in a material struct.

**The first question the building round answers is therefore which stage**, before any lobe:
what writes it, what reads it, and what it costs against 16.67 ms. *A stall is worse than a wrong
pixel, and a full-screen copy per transmissive draw is exactly the shape that stalls.*

## The obvious shape was refuted by the compiler, and the rule that refused it is right

The first design was: a `SceneBehind` resource, a `ResolveBehind` stage that reads `SceneHdr` and
writes it, and a `SubjectsTransmissive` stage that reads `SceneBehind` and contributes to `SceneHdr`.
**`static_assert(TopologicalOrderHolds())` refused it**, and its rule is one line:

> no stage at or after `s` may produce anything `s` reads

`ResolveBehind` reads `SceneHdr`; `SubjectsTransmissive` contributes to `SceneHdr` after it. **A reader
must see a finished resource, and a transmissive pass that adds to the very target its own input was
copied from does not give it one.** The invariant is load-bearing and is not what changes here.

**One thing this settles for free**: a transmissive pass is a SECOND PASS OF THE SUBJECTS UNIT and not
a sixth unit -- `Sky`, `Sun`, `Moon` and `Stars` are already four stages that are not four units. The
five geometry units of `CLAUDE.md` are untouched by any shape below.

## The two shapes that survive, with what each costs

| | |
|---|---|
| **every opaque stage contributes `SceneBehind` too** | no ordering problem, and **every frame pays it** -- a second Rgba16Float colour write across all opaque geometry, about 7 MB at 720p, whether or not the scene has any glass. *That breaks the property a `Content` stage is supposed to have: a plan that declares no glass pays nothing* |
| **the transmissive pass writes its OWN target and a composite merges** | `SubjectsTransmissive` reads `SceneHdr` and contributes to `SceneTransmissive`; a `CompositeTransmission` stage writes `SceneComposited` from the two, and `TemporalResolve` reads that instead. Every edge is clean, and `SceneComposited` **FallsBackTo `SceneHdr`** the way `SceneLinear` already does -- so a plan without glass pulls neither stage and pays nothing |

**The second is recommended** and the catalogue already carries the machinery for its one trick: an
alias for the resource a stage would otherwise have had to produce.

## The question it does not answer, and it is `TransmissionOrderTest`'s whole name

**A single composited layer cannot order two transmissive surfaces against each other.** The corpus
names the problem outright: `TransmissionOrderTest`. So the recommendation above is right for one
layer of glass and states nothing about two, and the round that builds it must say which of the four
transmission cases it can decide and which it cannot -- **before it builds, not after.**

## A cheaper path was looked for and ruled out by two measurements

**The shortcut considered**: a thin transmissive sheet with no volume needs no background texture,
because what passes through it is what alpha compositing already puts there -- so it could ride the
BLEND pipeline this unit already has, with no new stage at all.

**[MEASURED] it serves a minority.** Over the 148 models, transmissive materials split
**36 thin against 100 with a volume**. `TransmissionTest` is entirely thin and would be served whole;
`GlassVaseFlowers` and `TransmissionOrderTest` -- two of the four red glass cases -- are not.

**[MEASURED] and it cannot tint.** glTF transmits light TINTED BY BASE COLOUR, which is per-channel; a
single draw would have to carry both the reflected radiance and that tint, and SDL_GPU offers no
dual-source blend factor -- `SDL_GPU_BLENDFACTOR_SRC1_COLOR` does not exist in `SDL_gpu.h`. The only
single-target shapes left are `dst * srcColour` (the tint, losing the reflection) or premultiplied
`over` (the reflection, losing the tint). **A stained-glass window would come out colourless**, and
`StainedGlassLamp` is in the corpus.

*Two draws per transmissive triangle would close it and cost the frame twice for the same pixels.*

**So the background texture is the answer, and the detour is worth writing down because it was ruled
out with evidence rather than by taste**: it serves both the volumetric majority and the tint in one
draw, and it is the same resource refraction needs.

## Built, measured, and WITHDRAWN -- I could not finish it

**The renderer's half is in the tree and the reader that switches it on is not.** With the two
extensions read, the corpus went from 357 PASS to 279, **26 models red**, and the driver's own
validation **aborted 30 arms**. `CLAUDE.md`: *half-built is worse than not built*. The reader was
REMOVED rather than neutered -- a version behind a name that never matches is a dead path -- and
neither extension is in `kHonouredExtensions`, so a file that REQUIRES one is still refused by name.

[MEASURED] after the withdrawal the corpus is byte-identical to before it: 357 PASS, 0 SIGNAL,
criteria 132 of 138, 124 within the bound, the same 29 red.

## What is in the tree and is correct

The plan's two stages and two resources · the composite (premultiplied `over`, aliasing away with no
glass) · the material row carrying thickness and the Beer-Lambert pair · `SurfaceState` routing on
THICKNESS as the format requires · three transmissive fragment arms, one per vertex-layout family ·
the seventh texture slot and its pass-level binding · a second unit drawing the transmissive half of
the same draw list.

## The six layers, in the order they appeared, so the next round does not rediscover them

| # | what broke | what it really was |
|---|---|---|
| 1 | **444 of 444 arms failed at once** | the subject was mirrored into the second unit before the plan had pulled it, so a unit with no device refused -- *a guard on the wrong side of a question* |
| 2 | 26 models refused | the plan never ASKED for the transmissive pass; it is `Content` and must be declared |
| 3 | the composite was pruned | *nothing this plan requests reads what it draws into* -- with no temporal resolve, `SceneLinear` aliased past it |
| 4 | the alias did not reach | **the plan compiler did not chain aliases**: `SceneLinear -> SceneComposited -> SceneHdr` stopped at the first hop and a reader bound a resource the plan does not hold |
| 5 | 26 models still refused | the refusal was on BINDING a slot, not on drawing it. The opaque unit is handed the glass too and skips it; **refusing belongs to the case where NOBODY draws it**, which is a fact about the plan |
| 6 | **30 arms aborted** | the renderer handed every stage the FIRST geometry pass's attachment set. With two geometry passes the glass unit built pipelines declaring a colour attachment its pass does not set -- `board:1121`'s defect in a new place |

**Four, five and six were pre-existing and none of them could fire before.** Every one was an
assumption that held exactly as long as there was one subject pass, and a second instance of the same
unit is the first thing that tests them all at once. *Those three repairs stay in the tree; they are
right independently of transmission.*

## What is still unknown, and it is the whole of what remains

**After layer six the 30 aborts did not move.** Same count, same arms, same numbers. So there is at
least a seventh cause and I did not find it. *The next round starts by finding what the validated arm
aborts on -- with the reader restored on a single case rather than on the corpus, which is what I
should have done at layer one.*

## The seventh cause, found — and it was never about transmission

**The withdrawal ended with an instruction**: *find what the validated arm aborts on, with the reader
restored on a SINGLE case rather than on the corpus, which is what I should have done at layer one.*
That was unaffordable at eight minutes a run. It is **2.4 seconds** now (`board:1410`), and the answer
came in one.

```
For color attachment 0, the render pipeline's pixelFormat (MTLPixelFormatRGBA32Float)
does not match the framebuffer's pixelFormat (MTLPixelFormatRGBA16Float).
```

**`ScenePrecision::Float` upgraded two resources by NAME and there are four.** The compiler read
`SceneHdr` and `SceneLinear` out of a hand-written pair; `SceneTransmissive` was added for this feature
and the pair did not grow with it, so the glass pass's pipeline declared 32 bits against a 16-bit
target and Metal aborted the encoder outright. **A list that had to be remembered, and was not.**

**Repaired so the omission is unspellable**: `CarriesSceneRadiance` answers every resource
exhaustively and with no `default:`, so a resource added to the chain does not COMPILE until it says
which it is, and the compiler walks all of them. *The format could not stand in for the question and
that was checked before the function was written -- eight rows are declared `Rgba16Float` and only four
are radiance; the three atmosphere LUTs and the shading normal are not.*

**The same defect one level down**: `CompositeTransmissionStage` hardcoded `R16G16B16A16_FLOAT` for its
own target. It takes the plan's format now. *This is the third stage this round to be caught
hardcoding a scene format, after the tonemap and a temporal resolve -- so the class is real and the
repair is the plan being asked rather than the width being typed.*

**[MEASURED] the arm no longer aborts**: `TransmissionOrderTest`, 3 tests, **0 SIGNAL**, where it was
1 before.

## What the restored reader now measures, and it is two more questions

| | before the reader | with it |
|---|---|---|
| `picture_p99_delta_code` | -- | **0, PASS** |
| `worst_disagreement_px` | 2.5249835 | **60.154229** |
| `iou` | -- | 0.14126761 |

**The COLOUR is exact where the two agree a pixel is covered, and the COVERAGE is further apart than
when the glass was drawn opaque.** So what is wrong is not the lobe and not the composite's arithmetic;
it is which pixels the glass claims.

**And an eighth cause is already named by the catalogue itself.** A new check fails -- *every pixel we
drew names a surface slot of this subject's own table* -- and `RenderCatalogue.h` predicted it in as many
words: *it does NOT carry the shading normal or the surface identity: a glass fragment names no single
surface*. So the identity attachment is right to be empty there and the CHECK does not know it yet.

- [x] **The coverage a transmissive draw claims** is the oracle's within a pixel: `coverage_fraction`
  0.10575955 against 0.10576063, `iou` **0.9999487**, `pixels_disagreeing` **5**, `boundary_max_px` 1
- [x] **The identity check learns that a transmissive pixel names no slot**, which `RenderCatalogue.h`
  already stated and the harness now reads: the claim is asked of every class but `transmissive`, and a
  `subjectClass` of that name is what a case declares to say so

## The eighth cause, and the first condition for it was wrong

**The runner asked for the transmissive pass on a case that declares it is not about transmission.** A
coverage case replaces every surface with a flat emission and renders the oracle at zero transmission
bounces, so the oracle's glass is an opaque emitting body -- and this engine saw through it.

**The first repair asked whether the materials were the FILE'S, and that was the wrong question.**
[MEASURED] `ABeautifulGame` takes its colour from the file's own base-colour images through an
EMITTER at zero bounces, so it passed that test, drew its glass, and **entered the red set at
19.542392 px where it had never been** -- caught by diffing the red set case by case against the
previous run, which is the whole reason that diff is the rule.

**The question was always what the ORACLE was allowed to do**, and the case declares it:
`renders.default.bounces.transmission`. At zero the reference picture cannot show anything through a
surface, whatever the file's materials say. One condition now, read from the recipe, and it replaces
both the surface-table clearing and the pass request.

| | before the reader | with it, wrong condition | with it, right condition |
|---|---|---|---|
| `TransmissionOrderTest` `worst_disagreement_px` | 2.5249835 | 60.154229 | **2.5249835** |
| its `iou` | -- | 0.14126761 | **0.99814226** |
| its criterion | red | red | **met** |
| `ABeautifulGame` | green | **red, 19.542392 px** | **green** |
| `CommercialRefrigerator` | refused outright | green | **green** |

## The reader is back and NOTHING CAN SEE IT, and that is measured rather than assumed

[MEASURED] **0 of 145 cases declare `renders.default.bounces.transmission` above zero.** The corpus
renders at zero bounces throughout, because that is what makes Cycles evaluate the same coverage
predicate a centre-sampling rasteriser does -- so the transmissive pass is never requested and the
restored reader reaches no picture at all.

**So this is `board:1407`'s shape a second time**: the capability is built and correct, no case can see
it, and what is missing is a case configuration in which the ORACLE can decide it. The measurement that
matters is not another repair; it is finding that configuration.

**What such a case needs, and each is a real cost:**

- [x] **At least one transmission bounce** -- the recipe declares 8, and that was NOT sufficient on its
  own. See below: the preparer was blinding the oracle by a different door
- [x] **More than one sample per pixel** -- 512, and the cost of it is that two seeds no longer agree
  bit for bit, which is reduced per case and per metric with its reason. Because
  the corpus's 1 spp is what makes its coverage predicate exact, so this case is a DIFFERENT recipe
  rather than a changed one
- [x] **A subject small enough to afford it**: two generated cubes, twelve triangles each. `board:1363`'s route was a generated sphere for exactly
  this reason, and a generated pane of glass in front of a generated body is the same move
- [x] **The four cases that carry glass keep their present recipe** and stay coverage cases -- untouched, and re-run after the engine changed: `ABeautifulGame`, `CommercialRefrigerator`, `CompareTransmission` and `TransmissionOrderTest` are 12 of 12 PASS. What they
  decide is where the geometry is, and they decide it correctly today

**Until then the reader is a shortfall that is named rather than a claim**: it reads what the format
declares, no case draws it, and the four extensions' worth of picture below it is `board:1407`'s
generated-subject route waiting to be walked a second time.

## The case exists, and building it found two defects -- one in the preparer and one in the engine

**THE ORACLE WAS BLIND BY A DOOR NOBODY WAS LOOKING AT.** `no_surface_of_the_subject_is_a_light` takes
the subject out of the light tree so an emissive asset does not illuminate the scene it is being
measured in -- and it does that partly by setting `obj.visible_transmission = False` on every mesh. **A
ray that refracts through the shell was then forbidden to see the body behind it**, so the reference
came back a solid black cube whatever the recipe declared. It is now `transmissionBounces > 0`, read
from the same recipe field that already decides whether the transmissive pass is requested -- one
condition, in one place, for both.

**The route to that was three refutations, in this order.** The first render was black; the guess was
that the enclosed body's shadow rays were blocked by the shell, so the body was made an EMITTER --
still black, which refuted it. The second guess was that Blender had not imported the extension; a
probe printed `Transmission Weight = 1.0` and `IOR = 1.5` off the imported material, which refuted that
too. The third was that Cycles could not render it at all; the same file rendered directly in Cycles
with the same bounce settings put **(0.875, 0, 0)** at the centre pixel -- the emitter, seen through the
glass. *Only then was the difference between that probe and the preparer worth reading, and it was one
line.*

**AND THE ENGINE ADDED THE TRANSMITTED TERM WHERE THE EXTENSION REPLACES A TERM.**
`o.col = shaded.col + transmitted(...)` kept the surface's whole diffuse lobe and put the transmitted
radiance on top of it, so a shell with `transmissionFactor` 1 was drawn as a lit opaque grey box with a
pale blush of what was behind it. The extension's own composition is `mix(diffuse_brdf, specular_btdf,
transmission)`, so the diffuse half is displaced:

```
float3 diffuseColour = albedo * (1.0 - metalness) * (1.0 - surface.transmission);
```

| | before | after |
|---|---|---|
| `picture_p99_delta_code` | **243 codes** | **0** |
| `linear_channels_differing` | 292 398 | **468** of 292 404 |
| by eye | a grey box with a pink blush | the oracle's picture |

*One line, and it is the line the specification writes.* A material that declares no transmission
carries 0 and multiplies by one, which is why the corpus did not move.

## Comments

**The declared `ior` of 1.0 is the case's most interesting line and it is not a convenience.** glTF's
default index for a dielectric is 1.5, so a reader who left the extension out would still get 1.5 and
still get a refracting oracle. Declaring 1.0 states that this case is about transmission and not about
refraction, and a case about macroscopic refraction is `KHR_materials_volume`'s -- `board:1387`.

Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr materials transmission a surface light passes through**

A transmission factor and an optional texture, for glass and for anything a viewer sees THROUGH rather
than a surface a viewer sees. **The most common non-opaque surface in a built world is a window**, so
this is not an enhancement.

**Two models at the pin require it**; thirty-three use it -- the widest gap in the honoured list.

- [ ] What a transmissive surface does to the frame graph is decided BEFORE the lobe: it needs what is
  behind it, and that is a pass question and not a material one
- [ ] It composes with `KHR_materials_ior`, which the reader already honours
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

- [ ] **The coverage a transmissive draw claims**, which is the whole of the 60 px
- [ ] **The identity check learns that a transmissive pixel names no slot**, which the catalogue
  already states and the harness does not read

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

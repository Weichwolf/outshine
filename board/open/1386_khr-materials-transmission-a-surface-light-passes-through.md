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

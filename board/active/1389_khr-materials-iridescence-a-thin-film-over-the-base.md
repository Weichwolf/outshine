Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr materials iridescence a thin film over the base**

A thin-film interference layer -- an iridescence factor, an index of refraction and a thickness range --
which is what makes a soap bubble, an oil slick and a beetle's shell change hue with angle.

**Ten models use it**, including the two 346-body sweeps that forced the material colour RULE
(board:1362).

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_iridescence>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: data** (see the parent's table).

## What must be true

- [ ] The four declared numbers are read, refused when out of range, and reach the device
- [ ] The specular Fresnel becomes the film's, blended by the declared strength, and the base is
  weighted by `1 - max(F)` rather than channelwise -- the extension's `rgb_mix` and not the core's
  `fresnel_mix`
- [ ] A thickness of zero disables the layer, which the extension states outright
- [ ] The two halves take their coefficients from one set of constants
- [ ] The two textures are read -- **not this round**, and `board:1405` carries both

## Two defects in the extension's own implementation notes, both measured

**THE INVERSE FRESNEL MAP'S GUARD WALKS INTO THE POLE IT NAMES.** The text reads
`Fresnel0ToIor(baseF0 + 0.0001)`, commented *guard against 1.0*, and the addition carries the argument
PAST the pole rather than away from it -- the square root exceeds one and the denominator turns
negative. [MEASURED] F0 0.9 -> 38.01, 0.99 -> 402.0, 0.9999 -> inf, **1.0 -> -40002**. A metal whose
`baseColorFactor` is 1.0 is ordinary glTF, so this is reached by an asset rather than by a fuzzer.
Held below the pole instead, which is the map's own domain and carries no free parameter.

**A REFLECTANCE IS A FRACTION IN BOTH DIRECTIONS AND THE EXTENSION CLAMPS ONE.** `max(I, 0.0)` is its
only bound and the truncated Airy sum over a near-mirror base exceeds one -- [MEASURED] 1.19683 on
green, F0 0.766, cos 0.1. It cannot be left: the layering the same document specifies weights the base
by `1 - max(F)`, so a component above one turns the diffuse term negative and the surface emits.

## What was NOT provable, and it is named rather than waved past

**A degeneracy test does not exist for this model.** The obvious one -- film index equal to the medium
around it must give the plain Schlick curve -- is FALSE by construction, because Schlick at `F0 = 0`
returns `(1 - cos)^5` and not zero. [MEASURED] 0.455 worst deviation over twenty angles before the
test was withdrawn as wrong. The same argument kills film-index-equals-base-index. So the instrument
that stands in its place is a bound, a boundary and a movement: reflectance within `[0, 1]` over
500 094 channels, total internal reflection checked at Snell's own critical angle from both sides, and
a channel separation of 0.0466 over the extension's default thickness range -- because a function
returning a constant would satisfy every bound and show nothing.

## What this oracle can decide about iridescence, and it is less than the extension declares

[MEASURED] on Blender 5.2.0, by importing a glTF that declares all four layered extensions and reading
the sockets back off the Principled node the importer wired.

| declared | reaches Cycles as | verdict |
|---|---|---|
| `iridescenceIor` 1.4 | `Thin Film IOR` **1.4** | carried |
| `iridescenceThicknessMaximum` 380 | `Thin Film Thickness` **400.0** | **ignored** |
| `iridescenceThicknessMinimum` 120 | nothing | ignored |
| `iridescenceFactor` 0.8 | no socket exists | **inexpressible** |

**The thickness is 400.0 whatever the file says.** [MEASURED] over five declared ranges -- (120, 380),
(0, 800), (200, 200), (100, 400), (50, 600) -- the socket comes back 400.0 every time, unlinked. **The
harmless explanation was sought and ruled out**: the socket's factory default is 0.0, so the importer
DID write the value and wrote the extension's default maximum rather than the declared one. With no
thickness texture the extension's own rule makes the thickness the MAXIMUM, so this is wrong for every
maximum that is not 400.

**And there is no weight socket at all.** Blender's `Thin Film` is on or off; `iridescenceFactor` between
0 and 1 has nowhere to go, so a partial film is a picture this oracle cannot produce.

**What that leaves decidable**: a material declaring `iridescenceFactor` 1.0 and the default
`iridescenceThicknessMaximum` of 400.0, with no thickness texture. That is a real case and not an empty
set -- but it is narrower than the extension, and a case outside it carries a declared reduction naming
this measurement.

## The corpus is unchanged and that is the finding, not a null result

[MEASURED] full corpus before and after, same population both times: **453 tests, 366 PASS, criteria 135
of 141, 127 within the picture bound, 13 outside** -- and the 26 distinct red cases are **identical case
by case**. `CLAUDE.md`'s rule that a change altering the picture by design cannot reproduce it to six
decimals applies, and here it points the other way: **it says the extension reaches no pixel any case
compares.**

**107 of 148 cases replace every material with a flat emission**, 12 more with emission by material
index, 10 with a single emission and 7 with a Diffuse BSDF. **The 9 that shade declare none of the four
layered extensions.** `board:1363` records why the shading arm has never produced a case inside the
bound, and `board:1407` is the route out of it -- which is what this task now waits on for its picture.

**The same run rules out a regression from the refactor.** `shadeRow` lost six arguments and gained the
surface row, the lobe and the Fresnel became separately chosen, and the combination is now stated once
for all four paths -- and every case scored what it scored before.

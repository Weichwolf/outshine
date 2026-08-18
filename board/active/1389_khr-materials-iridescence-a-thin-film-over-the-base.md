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

## It has a picture now, and the residual has a named mechanism

`test/outshine/render/shaded-sphere-metal-iridescence` is the first case in this tree that SHADES this
extension. [MEASURED] p50 **0.029943872**, p99 **0.21558743**, outside the bound -- against the same
sphere without the film at p50 0.00011407057.

**The two sides compute different physics over a metal, by each one's own declaration.** Read from
`intern/cycles/kernel/closure/bsdf_microfacet.h`: Blender evaluates the film against a **complex** index
`(n, k)` estimated from `F0` and `F82` by Gulbrandsen's *Artist Friendly Metallic Fresnel*. The
extension inverts the REAL-index Fresnel and states the limit itself -- *this simple formula is used for
both dielectrics and metals. While it is physically correct for dielectric materials, it's only an
approximation for metals, which are usually described using a complex IOR with an additional extinction
factor. Such a value cannot be accurately inferred from the F0 value and is thus assumed to be 0.0.*

**So the metal case is the extension's own approximation meeting a renderer that does not make it.**
`test/outshine/render/shaded-sphere-black-iridescence` is the discriminator: a dielectric, where the
inversion is exact on both sides.

## The discriminator separated TWO terms, and the prediction was half wrong

**Written before the render**: *the residual collapses towards the black sphere's own 0.0038684683.*

| | p50 | p99 | bound |
|---|---|---|---|
| `shaded-sphere-black`, no film | 0.0038684683 | 0.0059721113 | within |
| **`shaded-sphere-black-iridescence`** | **0.028653333** | **0.047628166** | **within** |
| `shaded-sphere-metal-iridescence` | 0.029943872 | 0.21558743 | outside |

**The TAIL collapsed and the MEDIAN did not.** p99 fell by a factor of 4.5 from the metal to the
dielectric; p50 stayed at 2.9 % on both, seven times the film-free sphere's. **So the complex index
explains the tail and something else explains the median**, and the prediction that named one mechanism
for the whole residual is refuted.

**The dielectric case is INSIDE the bound**, so `KHR_materials_iridescence` now has a picture that holds
-- but a 2.9 % median with a named cause absent is not something to record as a pass and move past.

**Candidates for the median term, none asserted and none yet measured:**

- [ ] **The Airy sum is truncated at `m = 2`**, which is the extension's own truncation. Blender's
  `fresnel_iridescence_channel` may carry more orders or an exact series
- [ ] **The spectral integration.** The extension fits the CIE observer with three Gaussians plus a
  fourth lobe on X and normalises by `1.0685e-7`; a different fit, or a spectral renderer integrating
  properly, lands somewhere else by construction
- [ ] **Polarisation.** The extension approximates the phase step per interface as 0 or pi and drops the
  S/P split outright, saying so; Cycles' `fresnel_conductor_polarized` exists in the same file and its
  name says it does not
- [ ] **THE CAVEAT FIRST**: 2.9 % on a film whose colour swings through the whole hue circle is a SMALL
  disagreement about a large effect, so the harmless reading is that the two implementations are the
  same model at different fidelity. That reading is what the candidates above would confirm or break,
  and it is why this is a measurement to make rather than a defect to report

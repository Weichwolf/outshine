Type: task
State: active
Parent: 1382
Area: gltf
Tags: khronos

**Khr materials anisotropy a lobe that is not round**

An anisotropy strength and rotation, stretching the specular lobe along the tangent direction: brushed
metal, hair, vinyl. **It needs the tangent frame**, which the reader already builds and a shader test
already holds to the exporter's own construction.

**Seven models use it.**

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_anisotropy>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: data** (see the parent's table).

## Built, and the lobe was already most of the way there

**The same GGX with two roughnesses instead of one**, quoted from the extension: `at = mix(alpha, 1.0,
strength^2)` along the anisotropy direction and `ab = alpha` across it, with the isotropic half's own
Fresnel. **A strength of zero makes the two equal and the lobe round**, so no branch guards it and a
material declaring none reaches exactly the arithmetic it had.

**Its frame is the surface's own tangent, turned by the declared rotation** -- and a subject with no
tangent hands a zero vector rather than an invented direction, which is the format's rule and not a
convenience: *a mesh primitive using an anisotropy material MUST have a defined tangent space*. So the
lobe stays round on a mesh the extension says may not use it, and nothing is guessed.

**The four anisotropy cases score 30 checks with no failures**: `AnisotropyStrengthTest`,
`AnisotropyBarnLamp`, `AnisotropyDiscTest` and `CompareAnisotropy`.

- [x] The lobe is stated once in C++ and emitted from it, the way the metal-rough pair already is
- [x] It needs the tangent frame, which the reader already builds or refuses
- [ ] **`anisotropyTexture` is not read.** Red and green carry a direction in tangent space and blue a
  strength multiplier; a material declaring one is stretched by its FACTORS alone, which is the
  extension's own default texel `(1.0, 0.5, 1.0)` -- the +X direction at full strength

## Built, and blocked on a picture rather than on code

**The lobe is in the engine and both halves agree** -- `BrdfAnisotropicDistribution` and
`BrdfAnisotropicVisibility` beside their MSL twins, the rotation read in radians and applied in the
surface's own tangent frame.

**What blocks closing it is that nothing can see it.** Not one corpus case shades a material declaring
this extension: 107 of 148 replace every material with a flat emission, and the 9 that shade declare
none of the four layered extensions. So this task has no evidence under `test/` and the board's seventh
invariant is right to say so -- **it is unproven, and that is a true statement about the tree rather
than a bookkeeping gap.**

**`board:1407` is what unblocks it.** Anisotropy modifies the specular path and nothing else, a metal
has no diffuse term, and the diffuse term is the only thing `board:1363` measured this oracle as
modelling differently. [MEASURED] Blender's importer carries `anisotropyStrength` 0.65 and
`anisotropyRotation` 0.5 rad through to `Anisotropic` 0.65 and `Anisotropic Rotation` 0.0796 turns, so
the values arrive.

**One thing to check when the case is authored**: glTF states the direction in the mesh's own tangent
space, and Blender's `Tangent` socket came back UNLINKED on the import probe. Whether Principled's
default tangent is the mesh's UV tangent -- which is what glTF means -- is not established here.

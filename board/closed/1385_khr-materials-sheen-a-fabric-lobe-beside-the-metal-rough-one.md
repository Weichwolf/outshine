Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr materials sheen a fabric lobe beside the metal rough one**

A sheen colour and a sheen roughness on top of the metal-rough base, for cloth and for the retroreflective
rim a woven surface shows at grazing angles. **A data extension with a real lobe behind it** -- the
numbers alone are the parsed-field-waiting-for-a-shader this repository forbids.

**Two models at the pin require it**; ten use it.

- [ ] The lobe is stated once and twice: the MSL half emitted from the C++ half, held together by a
  shader test the way the metal-rough BRDF already is
- [ ] Energy is not created -- the sheen layer takes from what is below it
- [ ] Absence is the identity and costs no pipeline permutation

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_sheen>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: data** (see the parent's table).

## Comments

**IT HAS A PICTURE NOW AND THIS ORACLE CANNOT DECIDE IT.**
`test/outshine/render/shaded-sphere-metal-sheen` is the first case in this tree that shades this
extension. [MEASURED] p50 **0.27010314**, p99 **1.8482429**, well outside the bound.

**The cause is not this engine's lobe.** Read from `intern/cycles/kernel/closure/bsdf_sheen.h`, Blender
5.2.0 evaluates *Practical Multiple-Scattering Sheen Using Linearly Transformed Cosines* -- Zeltner,
Burley and Chiang, 2022 -- while `KHR_materials_sheen` specifies the ImageWorks "Charlie" lobe. **These
are two different BRDFs and not two approximations of one**: Blender's carries multiple scattering
between fibres and is fitted with linearly transformed cosines; the extension's is an analytic
`sin^(1/alpha)` distribution with its own tabulated albedo.

**So this is `board:1204`'s shape: an extension this oracle cannot decide**, and the rung is *reduce the
oracle* with the measurement beside it rather than *fix the engine*. Bending our lobe towards Blender's
would put this tree outside the extension it implements, to make a number smaller.

**What would still decide it**: a case whose criterion is the sheen layer's ENERGY rather than its
angular shape -- both models conserve, and the albedo scaling both apply is the same statement.
*Named, not built.*

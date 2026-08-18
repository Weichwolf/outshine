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

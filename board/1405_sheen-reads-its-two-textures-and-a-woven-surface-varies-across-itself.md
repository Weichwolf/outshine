Type: task
State: active
Parent: 1382
Area: gltf
Tags: khronos

**Sheen reads its two textures, and a woven surface varies across itself**

`KHR_materials_sheen` is built and honoured: the Charlie lobe, its visibility, and a directional albedo
integrated from that same lobe so the layered surface conserves energy. **What it reads are the two
FACTORS.** `sheenColorTexture` (RGB, sRGB-encoded) and `sheenRoughnessTexture` (alpha) are not read, so
a material declaring one is drawn with its factor across the whole surface.

**That is a plainer picture rather than a wrong one** -- it is exactly what the extension says a
material with no texture looks like -- but it is a shortfall and it is what makes a woven surface look
uniform where the file says it is not.

[MEASURED] at the pin: **10 of 42 sheen materials declare a texture**, and `SheenCloth` is made
entirely of them. `SheenTestGrid` and `SpecularSilkPouf`, the two that require the extension and score
green today, declare none — so the green is honest and this is what stands between it and complete.

## What must be true

- [ ] **Both textures reach the shader**, with the colour decoded as sRGB and the roughness taken from
  alpha, which is the extension's own reading of each
- [ ] **They ride the material's texture table** the way the six before them do, each with its own uv
  matrix and uv-set selector, so a second uv set works without a second path
- [ ] **A material declaring neither is bit-identical to today**, and that is checked rather than argued
- [ ] **`SheenCloth` is the case that decides it**, because it is the one model made of textured sheen

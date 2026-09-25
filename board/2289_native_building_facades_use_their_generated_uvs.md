Type: defect
State: active
Architecture: ready
Parent: 2169
Depends: 2290
Priority: P0
Area: generators, engine, render, materials
Tags: buildings, hockenheim, glsl, visual-acceptance

# Native building facades use their generated UVs

## Evidence and boundary

At Hockenheim lap 88 s, a long, near-camera building appears as a blank wall.
The vector source contains a single 24-point, about 352-metre footprint with
provider ID 344001642 and no building type. `StructureBake` assigns it a
9-metre default height and usually Fine LOD. The three observed copies were
successive snapshots of the same tile, not simultaneous duplicate geometry.
`BuildingMesh` already encodes facade style, frontage, bay and floor in UVs.
The former packed `StoredVertex` clamped both coordinates to [-4,4]; WI 2290
now preserves the encoded values before upload.
`TilePieces::Hands` sets no `Textured` flag and `Laying` registers only a flat
wall material, so those UVs are neither uploaded nor consumed. The builder's
large footprint still needs better massing after this material defect.
Most visible Box-LOD walls use `FaceUvX` instead of window coordinates, so a
shader alone leaves the Hockenheim 88-s image unchanged even when its material
mode reaches the GPU. The Box generator must emit scaled bay/floor UVs too.

## Executable architecture

The generator owns deterministic facade coordinates and building typology;
the native `Material` owns an explicit procedural surface mode. Add one
`Facade` mode to the engine material contract, validate it and pack it into
the existing per-draw material row. Only native building walls select it;
imported glTF and ordinary materials default to `None`. Roofs remain ordinary
metallic-roughness. The wall piece uploads UVs. Shader code decodes the
documented bay/floor/style encoding, computes antialiased masonry, glazing,
frames and ground-level entrances in linear space, and feeds base colour,
roughness and metalness into the same PBR BRDF. No atlas, image IO, extra
draw call, place/feature ID branch or hidden global shader switch.

Handle `FaceUvX` negative values as trim/plinth instead of windows. Finite
derivatives suppress subpixel flicker and distant repetition. Limit style
variance to a bounded palette; the image should read as designed architecture,
not a noisy texture. Procedural mode does not imply physical window openings
or interior collision: those are separate geometry/simulation obligations.
Keep material validation, CPU/GPU packing, shader reflection and public
documentation consistent. Test `None` as a negative control and a generated
facade at two distances/resolutions for stable pixels and bounded frame cost.
Fine and Box LODs share the same encoded UV contract. Box walls retain simple
per-edge bay repetition and floor height despite simplified geometry; all
other faces keep negative identifiers. Cap bay counts below the encoding
stride so a long edge cannot silently change the decoded style.

## Acceptance

- Hockenheim 88-s and a second town Place PNG are opened before/after.
  The wall has plausible facade rhythm and material response, without a
  missing road, dark pixels, obvious moire or major frame-budget regression.
- Direct glTF rendering and its Khronos material references remain unchanged;
  a wall generated without UVs fails explicitly instead of silently flattening.
- Native material validation, packing and shader reflection agree. Focused
  building/render suites, `make format`, `make lint` pass. Report p50/p95/p99,
  peak memory and actual residual architectural defects.

## Current measured step

Native `Facade` mode reaches the packed GPU row; Box and Fine LODs now supply
bounded bay/floor UVs, walls upload them, and the lit GLSL path shades windows,
frames, doors and plinth through the same metallic-roughness BRDF. An explicit
`None` negative control, malformed-mode validation, box/face UV checks, shader
reflection, 12 building cases, eight direct glTF renders, format and lint pass.
Diagnostic red shading proved the material branch was live; the initial 88-s
image nevertheless remained pixel-identical because Box walls had no bay UVs.
After fixing Box, the opened Hockenheim image changes 196,596/921,600 pixels;
the opened 640×360 image keeps legible windows without obvious moiré. The opened
Wien image gains distant facade structure, but its repeated grid is still
schematic. Hockenheim 100-s motion: 6000 frames, p50/p95/p99
1.977/7.792/13.073 ms, 18 above 16.667 ms; previous pinned motion file gives
1.993/7.862/12.702 ms and 15 late. These are single runs, not attribution.
End-of-run live C++ heap is 523,793,024 bytes and building-piece device storage
570,425,344 bytes; peak memory was not sampled.

Keep this WI active for a visual transfer check and material/geometry tuning.
The anonymous 352-metre footprint still becomes a monolithic block; generator
typology and source-tag provenance belong to WI 2173, not a shader exception.
The pinned MVT feature 344001642 has one 24-point ring, extent 4096 and a
232×905-unit bounding box in tile 14/8581/5603. A generic hall/terrace
decomposition correction in WI 2173 changed the opened 91.433-s Hockenheim
image by 0/921,600 pixels: that was not this facade's cause. Inspect the
actual building-use/roof result and source tags before altering facade style.
The dark noon facade also needs the lighting work in WI 2172. Different tile
landing counts at the same instant limit image-to-image numerical comparison;
WI 2230 owns capture snapshot reproducibility.

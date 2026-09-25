Type: defect
State: active
Architecture: ready
Parent: 2169
Depends:
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
`TilePieces::Hands` sets no `Textured` flag and `Laying` registers only a flat
wall material, so those UVs are neither uploaded nor consumed. The builder's
large footprint still needs better massing after this material defect.

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

## Acceptance

- Hockenheim 88-s and a second town Place PNG are opened before/after.
  The wall has plausible facade rhythm and material response, without a
  missing road, dark pixels, obvious moire or major frame-budget regression.
- Direct glTF rendering and its Khronos material references remain unchanged;
  a wall generated without UVs fails explicitly instead of silently flattening.
- Native material validation, packing and shader reflection agree. Focused
  building/render suites, `make format`, `make lint` pass. Report p50/p95/p99,
  peak memory and actual residual architectural defects.

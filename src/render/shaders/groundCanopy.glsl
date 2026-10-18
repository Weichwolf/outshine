uint groundCanopyRecord(uint row, uint rows) {
  if (row >= rows) { return 0u; }
  uint layersAt = floatBitsToUint(groundPalette[3]);
  uint size = uint(groundPalette.length());
  if (layersAt == 0u || layersAt > size || rows >= (size - layersAt) / 8u) { return 0u; }
  uint tail = layersAt + 8u * (rows + 1u);
  if (rows >= (size - tail) / 24u || (size - tail) != 24u * (rows + 1u)) { return 0u; }
  uint at = tail + row * 24u;
  float weight = groundPalette[at + 19u];
  if (!(weight > 0.0 && weight <= 1.0 && groundPalette[at + 11u] > 0.0 &&
        groundPalette[at + 15u] > 0.0 && groundPalette[at + 16u] > 0.0)) {
    return 0u;
  }
  return at;
}

GroundSurface groundSubstrateSurface(int which, vec2 atM, vec2 worldDx, vec2 worldDy, uint rows) {
  uint row = groundMaterialRow(which, rows);
  uint at = groundCanopyRecord(row, rows);
  if (at == 0u) { return groundSubstrateLayers(which, atM, worldDx, worldDy, rows, 0u); }
  vec4 shape = groundPaletteVector(at + 16u);
  float weight = shape.w;
  vec4 mean = groundWears(which, rows);
  vec4 green = groundPaletteVector(at + 8u);
  vec4 dry = groundPaletteVector(at + 12u);
  vec3 canopy = mix(green.rgb, dry.rgb, shape.z);
  uint surfacesAt = 4u * (rows + 2u) + rows + 1u;
  float contrast = clamp(groundPalette[surfacesAt + row * 8u + 4u], 0.0, 1.0);
  float footprintM = max(length(worldDx), length(worldDy));
  GroundDetail detail =
      substrateDetail(groundPaletteVector(at + 20u), contrast, atM, worldDx, worldDy, footprintM);
  GroundSurface result = GroundSurface(mean.rgb + weight * canopy * (detail.AlbedoScale - 1.0),
                                       mean.a + weight * detail.RoughnessOffset,
                                       weight * (1.0 + shape.y) * detail.BumpDxDy);
  if (weight >= 1.0) { return result; }
  uint substrateRow = groundSubstrateRow(which, rows);
  uint surfaceAt = surfacesAt + substrateRow * 8u;
  float coverage = clamp(groundPalette[surfaceAt + 7u], 0.0, 1.0);
  vec4 rawMean = mix(groundPaletteVector(at), groundPaletteVector(at + 4u), coverage);
  GroundSurface base = groundSubstrateLayers(which, atM, worldDx, worldDy, rows, at);
  result.Albedo += (1.0 - weight) * (base.Albedo - rawMean.rgb);
  result.Roughness += (1.0 - weight) * (base.Roughness - rawMean.a);
  result.BumpDxDy += (1.0 - weight) * base.BumpDxDy;
  return result;
}

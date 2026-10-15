#include "groundLitterFilter.glsl"
#include "groundLitterOriented.glsl"

struct GroundSurface {
  vec3 Albedo;
  float Roughness;
  vec2 BumpDxDy;
};

GroundSurface groundSurface(vec4 wears, GroundDetail detail) {
  return GroundSurface(
      wears.rgb * detail.AlbedoScale, wears.a + detail.RoughnessOffset, detail.BumpDxDy);
}

GroundSurface blendGroundSurfaces(GroundSurface first, GroundSurface second, float weight) {
  return GroundSurface(mix(first.Albedo, second.Albedo, weight),
                       mix(first.Roughness, second.Roughness, weight),
                       blendBumpDerivatives(first.BumpDxDy, second.BumpDxDy, weight));
}

GroundSurface groundSubstrateSurface(int which, vec2 atM, vec2 worldDx, vec2 worldDy, uint rows) {
  uint row = groundSubstrateRow(which, rows);
  uint at = 4u * (rows + 2u) + rows + 1u + row * 8u;
  vec4 recipe = vec4(
      groundPalette[at], groundPalette[at + 1u], groundPalette[at + 2u], groundPalette[at + 3u]);
  float contrast = clamp(groundPalette[at + 4u], 0.0, 1.0);
  float footprintM = max(length(worldDx), length(worldDy));
  uint layersAt = floatBitsToUint(groundPalette[3]);
  uint materialRow = groundMaterialRow(which, rows);
  float coverage = layersAt > 0u && materialRow < rows ? groundPalette[at + 7u] : 0.0;
  GroundDetail base = GroundDetail(1.0, 0.0, vec2(0.0));
  if (coverage < 1.0) {
    base = substrateDetail(recipe, contrast, atM, worldDx, worldDy, footprintM);
  }
  vec4 wears = groundWears(which, rows);
  if (coverage <= 0.0) { return groundSurface(wears, base); }
  uint layerAt = layersAt + materialRow * 8u;
  vec4 difference = vec4(groundPalette[layerAt],
                         groundPalette[layerAt + 1u],
                         groundPalette[layerAt + 2u],
                         groundPalette[layerAt + 3u]);
  vec4 litterRecipe = vec4(groundPalette[layerAt + 4u],
                           groundPalette[layerAt + 5u],
                           groundPalette[layerAt + 6u],
                           groundPalette[layerAt + 7u]);
  GroundDetail litter = substrateDetail(litterRecipe, contrast, atM, worldDx, worldDy, footprintM);
  if (coverage >= 1.0) { return groundSurface(wears, litter); }
  float field = recipe.z > 0.0 ? groundLitterFootprintField(atM, worldDx, worldDy, recipe.z) : 0.0;
  float weight = clamp(coverage + 2.0 * min(coverage, 1.0 - coverage) * field, 0.0, 1.0);
  vec4 first = wears - coverage * difference;
  return blendGroundSurfaces(
      groundSurface(first, base), groundSurface(first + difference, litter), weight);
}

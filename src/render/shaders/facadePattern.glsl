#include "periodicBand.glsl"
#include "roofTiles.glsl"
#include "facadeOpenings.glsl"
#include "facadeBands.glsl"
#include "facadeFootprint.glsl"
#include "facadeCoating.glsl"
#include "facadeProfiles.glsl"
#include "facadeMasonry.glsl"

void applyFacadeGlass(float coverage, inout vec3 albedo, inout float roughness) {
  FacadeGlazing glass = facadeGlazing(coverage, albedo, roughness);
  albedo = glass.albedo;
  roughness = glass.roughness;
}

void applyFacadeGlazedWindow(vec2 at, vec2 fractions, vec2 dx, vec2 dy,
                              inout vec3 albedo, inout float roughness) {
  vec3 coverage = facadeFootprintCoverage(fractions, at, dx, dy, 0.0);
  float pane = clamp(coverage.y / max(coverage.x, 0.0001), 0.0, 1.0);
  FacadeGlazing window = facadeBandedWindow(1.0, pane, 0.0, albedo, roughness);
  albedo = window.albedo;
  roughness = window.roughness;
}

void applyRoofCourses(vec2 encoded, vec3 positionM, vec3 localM, vec3 n, inout vec3 albedo,
                      inout float roughness, out vec2 heightChange) {
  heightChange = vec2(0.0);
  float slope = length(surfaceGradient(n, dFdx(positionM), dFdy(positionM),
                                       dFdx(encoded.y), dFdy(encoded.y)));
  if (slope < 0.05) { return; }
  float course = encoded.y / (0.32 * slope);
  vec2 courseChange = vec2(dFdx(encoded.y), dFdy(encoded.y)) / (0.32 * slope);
  float width = abs(courseChange.x) + abs(courseChange.y);
  vec3 axis = roofTileAxis(dFdx(localM), dFdy(localM), dFdx(encoded.y), dFdy(encoded.y));
  float tile = dot(localM, axis) / 0.26;
  vec2 tileChange = vec2(dot(dFdx(localM), axis), dot(dFdy(localM), axis)) / 0.26;
  RoofTileJoint joint = roofTileJoint(course, tile, width, abs(tileChange.x) + abs(tileChange.y));
  albedo *= mix(vec3(1.0), vec3(0.68, 0.70, 0.73), joint.coverage);
  roughness = mix(roughness, 1.0, joint.coverage);
  heightChange = -0.004 * (joint.courseSlope * courseChange + joint.tileSlope * tileChange);
}

void applyFacade(int code, vec2 coordinates, vec3 paint, vec3 positionM, vec3 localM, vec3 metricM, vec3 n,
                 bool metreMaterial, vec2 materialM, inout vec3 albedo, inout float roughness, out vec2 heightChange) {
  heightChange = vec2(0.0);
  vec2 dx = dFdx(coordinates);
  vec2 materialDx = dFdx(materialM);
  vec2 materialDy = dFdy(materialM);
  vec2 dy = dFdy(coordinates);
  vec3 dxM = dFdx(metricM);
  vec3 dyM = dFdy(metricM);
  float storeysPerM = length(surfaceGradient(n, dFdx(positionM), dFdy(positionM), dx.y, dy.y));
  if (code < 0) {
    float trim = float((-code - 1) % 16);
    float ident = float((-code - 1) / 16);
    if (trim > 10.5 && trim < 11.5) {
      applyFacadeGlass(1.0, albedo, roughness);
      return;
    }
    if (trim < 0.5) { albedo *= paint; return; }
    if (trim > 0.5 && trim < 2.5) {
      if (trim > 1.5) {
        albedo *= vec3(0.50, 1.05, 1.45);
        roughness = max(roughness, 0.90);
      } else {
        float variant = mod(ident, 8.0);
        vec3 tint = vec3(1.0);
        if (variant < 2.0) {
          tint = vec3(0.20, 0.43, 0.71);
          roughness = max(roughness, 0.78);
        } else if (variant < 4.0) {
          tint = vec3(0.60, 0.65, 0.75);
          roughness = max(roughness, 0.84);
        } else if (variant > 5.5 && variant < 6.5) {
          tint = vec3(0.85, 1.10, 1.10);
        } else if (variant > 6.5) {
          tint = vec3(0.65, 0.90, 1.12);
        }
        albedo *= tint;
        applyRoofCourses(coordinates, positionM, localM, n, albedo, roughness, heightChange);
      }
      return;
    }
    if (trim > 7.5 && trim < 8.5) {
      albedo *= vec3(0.68, 0.70, 0.72);
      roughness = max(roughness, 0.91);
    }
    return;
  }

  int style = code % kFacadeStyleCount;
  vec2 fractions = facadeFractions(style);
  int field = (code / kFacadeStyleCount) % 3;
  vec2 at = vec2(coordinates.x, coordinates.y - 1.0);
  if (style == 7 || field == 1) {
    applyFacadeGlazedWindow(at, fractions, dx, dy, albedo, roughness);
    return;
  }
  float variant = float(code / (kFacadeStyleCount * 3));
  vec3 wallTint = vec3(1.0);
  if (variant > 0.5 && variant < 1.5) { wallTint = vec3(0.90, 0.94, 1.00); }
  else if (variant < 2.5 && variant > 1.5) { wallTint = vec3(1.07, 1.05, 1.04); }
  else if (variant < 3.5 && variant > 2.5) { wallTint = vec3(0.90, 0.84, 0.74); }
  else if (variant < 4.5 && variant > 3.5) { wallTint = vec3(0.83, 0.88, 0.91); }
  else if (variant < 5.5 && variant > 4.5) { wallTint = vec3(1.00, 0.89, 0.84); }
  else if (variant < 6.5 && variant > 5.5) { wallTint = vec3(0.62, 0.43, 0.32); }
  else if (variant > 6.5) { wallTint = vec3(0.82, 0.69, 0.61); }
  albedo *= wallTint * paint;
  if (coordinates.y < 1.0 || fractions.x <= 0.0 || fractions.y <= 0.0) {
    fractions = vec2(0.0);
    storeysPerM = 0.0;
  }
  vec4 filtered = facadeCoatedFootprint(fractions, at, dx, dy, storeysPerM, metricM, dxM, dyM);
  bool masonry = metreMaterial && variant > 5.5 && variant < 6.5;
  float masonryField = 0.0;
  vec3 amplitude = vec3(0.0);
  float roughnessAmplitude = 0.0;
  if (masonry) {
    masonryField = facadeMasonryField(fractions, at, dx, dy, storeysPerM,
                               materialM, materialDx, materialDy, filtered.xyz);
    amplitude = facadeMasonryAmplitude(albedo, facadeCoatingAmplitude(albedo));
    roughnessAmplitude = max(min(0.04, min(roughness / 0.16, (1.0 - roughness) / 0.84)), 0.0);
  }
  FacadeGlazing wall = facadeCoatedWindow(filtered, albedo, roughness);
  if (masonry) {
    wall.albedo += amplitude * masonryField;
    wall.roughness += roughnessAmplitude * masonryField;
  }
  albedo = wall.albedo;
  roughness = wall.roughness;
}

void applyFacade(int code, vec2 coordinates, vec3 paint, vec3 positionM, vec3 localM, vec3 metricM, vec3 n,
                 inout vec3 albedo, inout float roughness, out vec2 heightChange) {
  applyFacade(code, coordinates, paint, positionM, localM, metricM, n,
               false, vec2(0.0), albedo, roughness, heightChange);
}

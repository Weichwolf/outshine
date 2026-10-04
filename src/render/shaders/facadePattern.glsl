#include "periodicBand.glsl"

void applyRoofCourses(vec2 encoded, vec3 positionM, vec3 n, inout vec3 albedo,
                      inout float roughness, out vec2 heightChange) {
  heightChange = vec2(0.0);
  float slope = length(surfaceGradient(n, dFdx(positionM), dFdy(positionM),
                                       dFdx(encoded.y), dFdy(encoded.y)));
  if (slope < 0.05) { return; }
  float course = encoded.y / (0.32 * slope);
  vec2 courseChange = vec2(dFdx(encoded.y), dFdy(encoded.y)) / (0.32 * slope);
  float width = abs(courseChange.x) + abs(courseChange.y);
  float joint = periodicBand(course, 0.0, 0.055, width);
  albedo *= mix(vec3(1.0), vec3(0.68, 0.70, 0.73), joint);
  roughness = mix(roughness, 1.0, joint);
  heightChange = -0.004 * periodicBandSlope(course, 0.0, 0.055, width) * courseChange;
}

void applyFacade(vec2 encoded, vec3 paint, vec3 positionM, vec3 n, inout vec3 albedo,
                 inout float roughness, out vec2 heightChange) {
  heightChange = vec2(0.0);
  if (encoded.x < 0.0) {
    float trim = mod(-encoded.x - 1.0, 16.0);
    float ident = floor((-encoded.x - 1.0) / 16.0);
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
        applyRoofCourses(encoded, positionM, n, albedo, roughness, heightChange);
      }
      return;
    }
    if (trim > 7.5 && trim < 8.5) {
      albedo *= vec3(0.68, 0.70, 0.72);
      roughness = max(roughness, 0.91);
    }
    return;
  }

  float variant = floor(encoded.x / (256.0 * 24.0));
  vec3 wallTint = vec3(1.0);
  if (variant > 0.5 && variant < 1.5) { wallTint = vec3(0.90, 0.94, 1.00); }
  else if (variant < 2.5 && variant > 1.5) { wallTint = vec3(1.07, 1.05, 1.04); }
  else if (variant < 3.5 && variant > 2.5) { wallTint = vec3(0.90, 0.84, 0.74); }
  else if (variant < 4.5 && variant > 3.5) { wallTint = vec3(0.83, 0.88, 0.91); }
  else if (variant < 5.5 && variant > 4.5) { wallTint = vec3(1.00, 0.89, 0.84); }
  else if (variant < 6.5 && variant > 5.5) { wallTint = vec3(0.62, 0.43, 0.32); }
  else if (variant > 6.5) { wallTint = vec3(0.82, 0.69, 0.61); }
  albedo *= wallTint * paint;
}

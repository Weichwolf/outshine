#include "periodicBand.glsl"

void applyFacade(vec2 encoded, inout vec3 albedo, inout float roughness,
                 inout float metalness) {
  if (encoded.x < 0.0) {
    float trim = mod(-encoded.x - 1.0, 16.0);
    float ident = floor((-encoded.x - 1.0) / 16.0);
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
      }
      return;
    }
    if (trim > 7.5 && trim < 8.5) {
      albedo *= vec3(0.68, 0.70, 0.72);
      roughness = max(roughness, 0.91);
    }
    return;
  }

  float group = floor(encoded.x / 256.0);
  float style = mod(group, 8.0);
  float standing = floor(mod(group, 24.0) / 8.0);
  float variant = floor(group / 24.0);
  float bay = encoded.x - group * 256.0;
  float storey = encoded.y - 1.0;
  float bayWidth = fwidth(bay);
  float storeyWidth = fwidth(storey);
  bool hall = style > 3.5 && style < 4.5;

  vec3 wallTint = vec3(1.0);
  if (variant > 0.5 && variant < 1.5) { wallTint = vec3(0.90, 0.94, 1.00); }
  else if (variant < 2.5 && variant > 1.5) { wallTint = vec3(1.07, 1.05, 1.04); }
  else if (variant < 3.5 && variant > 2.5) { wallTint = vec3(0.90, 0.84, 0.74); }
  else if (variant < 4.5 && variant > 3.5) { wallTint = vec3(0.83, 0.88, 0.91); }
  else if (variant < 5.5 && variant > 4.5) { wallTint = vec3(1.00, 0.89, 0.84); }
  else if (variant < 6.5 && variant > 5.5) { wallTint = vec3(0.62, 0.43, 0.32); }
  else if (variant > 6.5) { wallTint = vec3(0.82, 0.69, 0.61); }
  albedo *= wallTint;

  float joint = periodicBand(storey, 0.015, 0.040, storeyWidth);
  albedo *= mix(vec3(0.98), vec3(0.91, 0.92, 0.93), joint);

  float windowLow = hall ? 0.49 : 0.38;
  float windowHigh = hall ? 0.90 : 0.84;
  float innerX = periodicBand(bay, 0.18, 0.82, bayWidth);
  float innerY = periodicBand(storey, windowLow, windowHigh, storeyWidth);
  float outer = periodicBand(bay, 0.12, 0.88, bayWidth) *
                periodicBand(storey, windowLow - 0.04, windowHigh + 0.04, storeyWidth);
  float inner = innerX * innerY;
  float doorX = periodicBand(bay, 0.14, 0.86, bayWidth);
  float doorway = standing > 1.5 ? 1.0 : 0.0;
  float door = doorway * doorX * intervalBand(storey, 0.04, 0.91, storeyWidth);
  float innerDoor = doorway * innerX *
      intervalBand(storey, windowLow, windowHigh, storeyWidth);
  float outerDoor = doorway * doorX *
      intervalBand(storey, windowLow - 0.04, min(windowHigh + 0.04, 0.91), storeyWidth);
  float window = max(inner - innerDoor, 0.0);
  float frame = max(outer - inner - outerDoor + innerDoor, 0.0);

  vec3 glass = albedo * vec3(0.22, 0.30, 0.39);
  vec3 metal = albedo * vec3(0.58, 0.62, 0.65);
  vec3 doorColour = albedo * vec3(0.31, 0.36, 0.39);
  albedo = albedo * max(1.0 - window - frame - door, 0.0) +
           glass * window + metal * frame + doorColour * door;
  roughness = roughness * (1.0 - window - frame) + 0.26 * window + 0.48 * frame;
  metalness = metalness * (1.0 - frame) + 0.42 * frame;
}

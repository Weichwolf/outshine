#version 450
#extension GL_GOOGLE_include_directive : require
#include "Parameters.inc"
layout(location = 0) out vec4 colour;
void main() {
  uvec2 at = uvec2(gl_FragCoord.xy);
  at.y = min(at.y, CALIBRATION_HEIGHT - 1u - at.y);
  uint value = at.x + CALIBRATION_WIDTH * at.y;
  for (uint step = 0; step < CALIBRATION_RASTER_STEPS; ++step) {
    value = value * CALIBRATION_MIX_MULTIPLIER + CALIBRATION_MIX_INCREMENT;
    value ^= value >> 13u;
  }
  colour = vec4(float(value & 255u), float((value >> 8u) & 255u),
                float((value >> 16u) & 255u), 255) / 255;
}

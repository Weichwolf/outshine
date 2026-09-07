#version 450
#extension GL_GOOGLE_include_directive : require
#define DISPLAY_BINDING 1
#include "display.glsl"
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D sceneDepth;
layout(set = 2, binding = 2) uniform sampler2D history;
layout(set = 2, binding = 3) uniform sampler2D velocity;
layout(std140, set = 3, binding = 0) uniform Temporal {
  vec2 jitterDelta;
  vec2 texel;
  float historyHeld;
  float pad0, pad1, pad2;
} u;
layout(location = 0) out vec4 linearColour;
layout(location = 1) out vec4 displayColour;
const float kCurrentWeight = 0.1;

vec3 rgbToYCoCg(vec3 c) {
  return vec3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b,
              -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

vec3 yCoCgToRgb(vec3 c) {
  float t = c.x - c.z;
  return vec3(t + c.y, c.x + c.z, t - c.y);
}

vec3 clipTowards(vec3 historyColour, vec3 centre, vec3 extent) {
  vec3 offset = historyColour - centre;
  vec3 unit = abs(offset) / max(extent, vec3(1.0e-5));
  float largest = max(max(unit.x, unit.y), unit.z);
  return largest > 1.0 ? centre + offset / largest : historyColour;
}

void main() {
  uvec2 px = uvec2(gl_FragCoord.xy);
  vec4 here = texelFetch(scene, ivec2(px), 0);

  vec3 lowest = vec3(1.0e30);
  vec3 highest = vec3(-1.0e30);
  vec3 total = vec3(0.0);

  ivec2 limit = ivec2(int(textureSize(scene, 0).x) - 1, int(textureSize(scene, 0).y) - 1);
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      uvec2 at = uvec2(clamp(ivec2(px) + ivec2(dx, dy), ivec2(0), limit));
      vec3 neighbour = rgbToYCoCg(texelFetch(scene, ivec2(at), 0).rgb);
      lowest = min(lowest, neighbour);
      highest = max(highest, neighbour);
      total += neighbour;
    }
  }
  vec3 centre = total * (1.0 / 9.0);
  vec3 extent = max(highest - centre, centre - lowest);

  vec2 motion = texelFetch(velocity, ivec2(px), 0).xy - u.jitterDelta * u.texel;
  vec2 uv = (vec2(px) + 0.5) * u.texel;
  vec2 was = uv - motion;

  vec3 kept = here.rgb;
  bool inside = was.x >= 0.0 && was.x <= 1.0 && was.y >= 0.0 && was.y <= 1.0;
  if (inside && u.historyHeld > 0.5) {
    vec3 past = rgbToYCoCg(texture(history, was).rgb);
    kept = mix(yCoCgToRgb(clipTowards(past, centre, extent)), here.rgb, kCurrentWeight);
  }

  linearColour = vec4(kept, here.a);
  displayColour = displayed(linearColour, texelFetch(sceneDepth, ivec2(px), 0).r);
}

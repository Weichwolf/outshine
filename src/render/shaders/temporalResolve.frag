#version 450
#extension GL_GOOGLE_include_directive : require
#define DISPLAY_BINDING 1
#include "display.glsl"
#include "temporalReprojection.glsl"
#define SCENE_FLOAT(name, value) const float name = value;
#include "render/stages/SceneConstants.inc"
#undef SCENE_FLOAT
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D sceneDepth;
layout(set = 2, binding = 2) uniform sampler2D history;
layout(set = 2, binding = 3) uniform sampler2D velocity;
layout(std140, set = 3, binding = 0) uniform Temporal {
  vec2 jitterDelta;
  vec2 texel;
  float historyHeld;
  float pad0, pad1, pad2;
  mat4 previousClipFromCurrentClip;
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

  float lowestCoverage = 1.0;
  float highestCoverage = 0.0;
  vec3 lowest = vec3(1.0e30);
  vec3 highest = vec3(-1.0e30);
  vec3 total = vec3(0.0);

  ivec2 limit = ivec2(int(textureSize(scene, 0).x) - 1, int(textureSize(scene, 0).y) - 1);
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      uvec2 at = uvec2(clamp(ivec2(px) + ivec2(dx, dy), ivec2(0), limit));
      vec4 neighbourSample = texelFetch(scene, ivec2(at), 0);
      vec3 neighbour = rgbToYCoCg(neighbourSample.rgb);
      lowestCoverage = min(lowestCoverage, neighbourSample.a);
      highestCoverage = max(highestCoverage, neighbourSample.a);
      lowest = min(lowest, neighbour);
      highest = max(highest, neighbour);
      total += neighbour;
    }
  }
  vec3 centre = total * (1.0 / 9.0);
  vec3 extent = max(highest - centre, centre - lowest);

  vec2 motionNdc = texelFetch(velocity, ivec2(px), 0).xy;
  vec2 uv = (vec2(px) + 0.5) * u.texel;
  vec2 jitterUv = u.jitterDelta * u.texel * vec2(1.0, -1.0);
  vec2 was = temporalHistoryUv(uv, motionNdc, jitterUv);
  float depth = texelFetch(sceneDepth, ivec2(px), 0).r;
  if (depth == 0.0 && all(equal(motionNdc, vec2(kVelocityStatic)))) {
    vec4 previous = u.previousClipFromCurrentClip * vec4(uv * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);
    was = previous.w > 0.0 ? previous.xy / previous.w * vec2(0.5, -0.5) + 0.5 + jitterUv : vec2(-1.0);
  }

  vec3 kept = here.rgb;
  float coverage = here.a;
  bool inside = was.x >= 0.0 && was.x <= 1.0 && was.y >= 0.0 && was.y <= 1.0;
  if (inside && u.historyHeld > 0.5) {
    vec4 historySample = texture(history, was);
    vec3 past = rgbToYCoCg(historySample.rgb);
    coverage = mix(clamp(historySample.a, lowestCoverage, highestCoverage), here.a, kCurrentWeight);
    kept = mix(yCoCgToRgb(clipTowards(past, centre, extent)), here.rgb, kCurrentWeight);
  }

  linearColour = vec4(kept, coverage);
  displayColour = displayed(linearColour, 0.0);
}

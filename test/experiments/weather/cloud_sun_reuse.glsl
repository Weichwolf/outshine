#ifndef OUTSHINE_CLOUD_SUN_REUSE_GLSL
#define OUTSHINE_CLOUD_SUN_REUSE_GLSL

#include "cloudLayer.glsl"
#include "source_sun_maps.glsl"

layout(set = 0, binding = 4) uniform sampler2D solarSourceField;

uint solarSourceDensity = 0u;
uint solarSourceTextures = 0u;

float sourceDirectSolarTransfer(CloudLayer layer, vec3 pointM, vec3 toSun) {
#if SOURCE_SOLAR_DIAGNOSTICS
  if (layer.field.x > 0.0) {
    mat2 spans = cloudShell(layer, pointM, toSun, 1.0e8);
    for (int index = 0; index < 2; ++index) {
      if (spans[index].y > spans[index].x) { solarSourceDensity += uint(CLOUD_LIGHT_SAMPLES); }
    }
  }
#endif
  return exp(-cloudOpticalDepthSteps(layer, pointM, toSun, 1.0e8, CLOUD_LIGHT_SAMPLES));
}

vec2 sourceSolarLevel(vec2 uv, uint level) {
  SourceSolarMap frame = sourceSolarMaps[sourceSolarScene(gl_GlobalInvocationID.x) * 3u + level];
  vec2 coordinate = (uv - frame.OriginUv) / frame.StepUv;
  vec2 maximum = vec2(frame.Extent) - vec2(1.0);
  if (any(lessThan(coordinate, vec2(0.0))) || any(greaterThanEqual(coordinate, maximum))) {
    return vec2(0.0);
  }
  vec2 edge = min(coordinate, maximum - coordinate);
  float weight = clamp(min(edge.x, edge.y) / 16.0, 0.0, 1.0);
  if (weight <= 0.0) { return vec2(0.0); }
  vec2 texel = vec2(frame.AtlasOrigin) + coordinate + vec2(0.5);
  float through = textureLod(solarSourceField, texel / vec2(textureSize(solarSourceField, 0)), 0.0).r;
#if SOURCE_SOLAR_DIAGNOSTICS
  ++solarSourceTextures;
#endif
  return vec2(through, weight);
}

float sourceReusedSolarTransfer(CloudLayer layer, vec3 pointM, vec3 toSun) {
#if SOURCE_SOLAR_REUSE
  float height = cloudAltitude(pointM, layer.shell.x);
  float fade = clamp((layer.shell.y - height) / 100.0, 0.0, 1.0);
  float remaining = fade;
  float through = 0.0;
  if (remaining > 0.0) {
    vec3 horizontal = normalize(cross(toSun, vec3(0.0, 0.0, 1.0)));
    vec3 vertical = cross(horizontal, toSun);
    vec2 uv = vec2(dot(pointM, horizontal), dot(pointM, vertical));
    for (uint level = 0u; level < 3u && remaining > 0.0; ++level) {
      vec2 sampled = sourceSolarLevel(uv, level);
      through += remaining * sampled.y * sampled.x;
      remaining *= 1.0 - sampled.y;
    }
  }
  float directWeight = 1.0 - fade + remaining;
  if (directWeight > 0.0) {
    through += directWeight * sourceDirectSolarTransfer(layer, pointM, toSun);
  }
  return through;
#else
  return sourceDirectSolarTransfer(layer, pointM, toSun);
#endif
}

#endif

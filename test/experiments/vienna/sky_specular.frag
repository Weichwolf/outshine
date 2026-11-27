#version 410 core

const float kPi = 3.14159265359;
#include "environmentSampling.glsl"
#include "metalRoughBrdf.glsl"

uniform sampler2D skyRadiance;
uniform bool integrateBrdf;
uniform vec2 extent;
out vec4 colour;

vec2 samplePoint(int index, int count) {
  return vec2((float(index) + .5) / float(count),
              float(bitfieldReverse(uint(index))) * 2.3283064365386963e-10);
}

vec3 environmentRadiance(vec3 direction) {
  float longitude = dot(direction.xy, direction.xy) > 1.e-10
      ? atan(direction.y, direction.x) : 0.;
  vec2 uv = vec2(.5 - longitude / (2. * kPi),
                 .5 + asin(clamp(direction.z, -1., 1.)) / kPi);
  return textureLod(skyRadiance, uv, 0.).rgb;
}

vec3 filteredRadiance(vec3 axis, int level) {
  if (level == 0)
    return environmentRadiance(axis);
  float roughness = float(level) / float(kEnvironmentSpecularLevels - 1);
  vec3 total = vec3(0.);
  float weight = 0.;
  for (int i = 0; i < kEnvironmentSamples; ++i) {
    vec3 halfVector = environmentHalfVector(samplePoint(i, kEnvironmentSamples), roughness, axis);
    vec3 light = reflect(-axis, halfVector);
    float nl = max(dot(axis, light), 0.);
    total += environmentRadiance(light) * nl;
    weight += nl;
  }
  return total / max(weight, 1.e-6);
}

vec2 integrateReflectance(float roughness, float nv) {
  const int count = 512;
  vec3 normal = vec3(0., 0., 1.);
  vec3 view = vec3(sqrt(1. - nv * nv), 0., nv);
  float a2 = pow(roughness, 4.);
  vec2 total = vec2(0.);
  for (int i = 0; i < count; ++i) {
    vec3 halfVector = environmentHalfVector(samplePoint(i, count), roughness, normal);
    float vh = dot(view, halfVector);
    float nl = reflect(-view, halfVector).z;
    if (vh <= 0. || nl <= 0.)
      continue;
    // Cancel the GGX distribution against the reflected half-vector density.
    float weight = 4. * nl * brdfVisibility(nl, nv, a2) * vh / halfVector.z;
    float fresnel = pow(1. - clamp(vh, 0., 1.), 5.);
    total += weight * vec2(1. - fresnel, fresnel);
  }
  return total / float(count);
}

void main() {
  if (integrateBrdf) {
    vec2 parameters = gl_FragCoord.xy / extent;
    colour = vec4(integrateReflectance(parameters.x, parameters.y), 0., 1.);
    return;
  }
  int span = kEnvironmentSide + 2 * kEnvironmentBorder;
  int row = int(gl_FragCoord.y);
  int level = row / span;
  if (level >= kEnvironmentSpecularLevels)
    discard;
  vec2 uv = (vec2(gl_FragCoord.x, float(row % span) + .5) - float(kEnvironmentBorder)) /
            float(kEnvironmentSide);
  colour = vec4(filteredRadiance(octDirection(uv), level), 1.);
}

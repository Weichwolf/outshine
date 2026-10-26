#ifndef OUTSHINE_CLOUD_CUMULUS_GLSL
#define OUTSHINE_CLOUD_CUMULUS_GLSL

float cloudCumulusUnit(float value) {
  float bounded = clamp(value, 0.0, 1.0);
  return bounded * bounded * (3.0 - 2.0 * bounded);
}

float cloudCumulusShape(vec3 advected, float height, float cover) {
  ivec2 cell = ivec2(floor(advected.xz));
  vec2 local = advected.xz - vec2(cell);
  float radiusScale = 0.7 * sqrt(cover);
  float body = 0.0;
  for (int x = -1; x <= 1; ++x) {
    for (int z = -1; z <= 1; ++z) {
      uint bits = cloudHashBits(ivec3(cell.x + x, 812, cell.y + z));
      vec4 feature = vec4((uvec4(bits) >> uvec4(0u, 8u, 16u, 24u)) & uvec4(255u)) / 255.0;
      vec2 center = vec2(x, z) + 0.5 + (feature.xy - 0.5) * 0.6;
      float radius = radiusScale * (0.85 + 0.3 * feature.z);
      float top = 0.55 + 0.4 * feature.w;
      vec2 delta = (local - center) / radius;
      float vertical = height / top;
      body = max(body, 1.0 - dot(delta, delta) - vertical * vertical);
    }
  }
  return body;
}

float cloudCumulusProfile(float height, float eroded) {
  return 1.5 * cloudCumulusUnit(height / 0.08) * cloudCumulusUnit(eroded);
}

float cloudCumulusDensity(vec3 advected, float height, float cover) {
  float body = cloudCumulusShape(advected, height, cover);
  if (body <= 0.0) { return 0.0; }
  float detail = cloudNoise(advected * 3.1 + vec3(19.0, 7.0, 3.0));
  return cloudCumulusProfile(height, body - 0.18 * (1.0 - detail));
}

#endif

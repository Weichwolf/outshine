#ifndef OUTSHINE_CLOUD_CUMULUS_GLSL
#define OUTSHINE_CLOUD_CUMULUS_GLSL

float cloudCumulusUnit(float value) {
  float bounded = clamp(value, 0.0, 1.0);
  return bounded * bounded * (3.0 - 2.0 * bounded);
}

float cloudCumulusShape(vec3 advected, float height, float cover, uvec3 origin) {
  ivec2 cell = ivec2(floor(advected.xz));
  vec2 local = advected.xz - vec2(cell);
  float radiusScale = 0.7 * sqrt(cover);
  float body = 0.0;
  for (int x = -1; x <= 1; ++x) {
    for (int z = -1; z <= 1; ++z) {
      uvec2 globalCell = uvec2(cell + ivec2(x, z)) + origin.xz;
      uint bits = cloudHashBits(ivec3(globalCell.x, 812u, globalCell.y));
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

#endif

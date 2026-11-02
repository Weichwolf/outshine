struct SectorMoments {
  vec4 MassNormal;
  float Roughness4;
  mat3 ColourNormal;
};

SectorMoments filteredSector(int which, uint sector) {
  uint queryAt = word(322u) + uint(which) * 8u;
  vec2 uv = vec2(data[queryAt], data[queryAt + 1u]);
  float lod = data[queryAt + 6u];
  uint base = uint(floor(lod));
  SectorMoments result = SectorMoments(vec4(0.0), 0.0, mat3(0.0));
  for (uint mip = 0u; mip < 2u; ++mip) {
    uint level = min(base + mip, word(320u) - 1u);
    float amount = mip == 0u ? 1.0 - fract(lod) : fract(lod);
    if (amount == 0.0) { continue; }
    uint header = word(321u) + level * 4u;
    uint width = word(header);
    vec2 point = uv * float(width) - 0.5;
    ivec2 low = ivec2(floor(point));
    vec2 t = fract(point);
    for (uint corner = 0u; corner < 4u; ++corner) {
      ivec2 offset = ivec2(corner & 1u, corner >> 1u);
      ivec2 xy = clamp(low + offset, ivec2(0), ivec2(int(width) - 1));
      float weight = amount * (offset.x == 0 ? 1.0 - t.x : t.x) * (offset.y == 0 ? 1.0 - t.y : t.y);
      uint at = word(header + 2u) + (uint(xy.y) * width + uint(xy.x)) * 56u + sector * 14u;
      result.MassNormal += weight * readFour(at);
      result.Roughness4 += weight * data[at + 4u];
      result.ColourNormal += weight * mat3(vec3(data[at + 5u], data[at + 6u], data[at + 7u]),
                                           vec3(data[at + 8u], data[at + 9u], data[at + 10u]),
                                           vec3(data[at + 11u], data[at + 12u], data[at + 13u]));
    }
  }
  return result;
}

vec3 shadeMoments(int which, uint mode) {
  vec3 l = normalize(mode == 3u ? vec3(0.5, -0.2, 0.8) : vec3(-0.4, 0.2, 0.8));
  vec3 v = normalize(vec3(0.0, 0.6, 1.0)), h = normalize(l + v);
  float vh = max(dot(v, h), 0.0);
  float fresnel = 0.04 + 0.96 * pow(1.0 - vh, 5.0);
  vec3 result = vec3(0.0);
  for (uint sector = 0u; sector < 4u; ++sector) {
    SectorMoments moments = filteredSector(which, sector);
    float mass = moments.MassNormal.x;
    if (mass <= 0.0) { continue; }
    vec3 mean = moments.MassNormal.yzw / mass;
    float normalLength = min(length(mean), 1.0);
    vec3 n = normalize(mean);
    float a2 = clamp(moments.Roughness4 / mass, 0.00000256, 1.0);
    float d = normalLength * a2 + 2.0 * (1.0 - a2) * (1.0 - normalLength);
    float filteredA2 = 2.0 * d / (2.0 * normalLength * (1.0 - a2) + 2.0 * d);
    float nl = max(dot(n, l), 0.0);
    Brdf brdf = metalRoughBrdf(vec3(0.0),
                               vec3(0.04),
                               1.0,
                               filteredA2,
                               nl,
                               max(dot(n, v), 1.0e-6),
                               max(dot(n, h), 0.0),
                               vh);
    result += (1.0 - fresnel) * max(transpose(moments.ColourNormal) * l, vec3(0.0)) / kPi +
              mass * brdf.specular * nl;
  }
  return result;
}

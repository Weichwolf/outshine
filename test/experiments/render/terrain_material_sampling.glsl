struct Sample {
  vec4 Material;
  vec4 GradientAddress;
};

uint word(uint at) {
  return floatBitsToUint(data[at]);
}

vec4 readFour(uint at) {
  return vec4(data[at], data[at + 1u], data[at + 2u], data[at + 3u]);
}

Sample sampleMaterial(vec4 uniforms, int which) {
  if (which < 0 || uint(which) >= word(323u)) { return Sample(vec4(0.0), vec4(0.0)); }
  uint queryAt = word(322u) + uint(which) * 8u;
  vec4 horizontal = readFour(queryAt);
  vec2 vertical = vec2(data[queryAt + 4u], data[queryAt + 5u]);
  float lod = data[queryAt + 6u];
  uint level = uint(floor(lod)) + uint(uniforms.x < fract(lod));
  uint header = word(321u) + level * 4u;
  uint width = word(header), capacity = word(header + 1u);
  precise vec2 at =
      horizontal.xy + (uniforms.y - 0.5) * horizontal.zw + (uniforms.z - 0.5) * vertical;
  uvec2 xy = min(uvec2(floor(at * float(width))), uvec2(width - 1u));
  uint cell = xy.y * width + xy.x;
  uint first = word(header + 2u) + cell * capacity * 8u;
  float total = 0.0;
  for (uint i = 0u; i < capacity; ++i) { total += data[first + i * 8u + 6u]; }
  precise float wanted = uniforms.w * total;
  float cumulative = 0.0;
  uint chosen = capacity - 1u;
  for (uint i = 0u; i < capacity; ++i) {
    cumulative += data[first + i * 8u + 6u];
    if (wanted < cumulative) {
      chosen = i;
      break;
    }
  }
  uint selected = first + chosen * 8u;
  return Sample(
      readFour(selected),
      vec4(
          data[selected + 4u], data[selected + 5u], float(level), float(cell * capacity + chosen)));
}

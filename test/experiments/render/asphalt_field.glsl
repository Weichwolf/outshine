uint asphaltHash(ivec2 cell) {
  uvec2 p = uvec2(cell);
  uint h = p.x * 0x9e3779b9u ^ p.y * 0x85ebca6bu;
  h ^= h >> 16u;
  h *= 0x7feb352du;
  h ^= h >> 15u;
  h *= 0x846ca68bu;
  return h ^ (h >> 16u);
}

float asphaltCross(vec2 a, vec2 b) {
  return a.x * b.y - a.y * b.x;
}

float asphaltGaussian(vec2 at, vec2 dx, vec2 dy) {
  const float variance = 0.0144;
  float trace = (dot(dx, dx) + dot(dy, dy)) / 12.0;
  float cross = asphaltCross(dx, dy) / 12.0;
  float determinant = cross * cross;
  float narrow = variance * variance + variance * trace + determinant;
  float broad = 16.0 * variance * variance + 4.0 * variance * trace + determinant;
  vec2 amplitudes = variance * inversesqrt(vec2(narrow, broad));
  ivec2 cell = ivec2(floor(at));
  float result = 0.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      ivec2 neighbour = cell + ivec2(x, y);
      uint bits = asphaltHash(neighbour);
      vec2 jitter = 0.2 + 0.6 * vec2(bits & 1023u, (bits >> 10u) & 1023u) / 1023.0;
      float weight = 0.75 + 0.5 * float((bits >> 20u) & 1023u) / 1023.0;
      vec2 delta = at - vec2(neighbour) - jitter;
      float square = dot(delta, delta);
      vec2 projected = vec2(asphaltCross(dx, delta), asphaltCross(dy, delta));
      float lateral = dot(projected, projected) / 12.0;
      vec2 exponent = -0.5 * vec2(variance * square + lateral, 4.0 * variance * square + lateral) /
                      vec2(narrow, broad);
      vec2 kernels = amplitudes * exp(exponent);
      result += weight * (kernels.x - kernels.y);
    }
  }
  return result;
}

float asphaltVisibility(vec2 dx, vec2 dy) {
  float trace = dot(dx, dx) + dot(dy, dy);
  float cross = asphaltCross(dx, dy);
  float determinant = cross * cross;
  float divisor = trace + sqrt(max(trace * trace - 4.0 * determinant, 0.0));
  float minor = divisor > 0.0 ? sqrt(2.0 * determinant / divisor) : 0.0;
  return 1.0 - smoothstep(2.0, 4.0, minor);
}

float asphaltFiltered(vec2 at, vec2 dx, vec2 dy) {
  float visibility = asphaltVisibility(dx, dy);
  if (visibility == 0.0) { return 0.0; }
  ivec2 subdivisions = ivec2(2);
  if (dot(dx, dx) > 4.0 * dot(dy, dy)) {
    subdivisions = ivec2(4, 1);
  } else if (dot(dy, dy) > 4.0 * dot(dx, dx)) {
    subdivisions = ivec2(1, 4);
  }
  vec2 step = 1.0 / vec2(subdivisions);
  float result = 0.0;
  for (int y = 0; y < subdivisions.y; ++y) {
    for (int x = 0; x < subdivisions.x; ++x) {
      vec2 offset = (vec2(x, y) + 0.5) * step - 0.5;
      result += asphaltGaussian(at + offset.x * dx + offset.y * dy, dx * step.x, dy * step.y);
    }
  }
  return result * visibility / 4.0;
}

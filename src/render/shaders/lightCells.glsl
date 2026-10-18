layout(std430, set = 2, binding = 24) readonly buffer WorldEmitters { Light worldEmitters[]; };
layout(std430, set = 2, binding = 25) readonly buffer WorldLightIndex { uint lightWords[]; };
layout(std140, set = 3, binding = 4) uniform WorldLightFrame {
  ivec4 positionCell;
  vec4 positionLocal;
  ivec4 lookupCell;
} lightFrame;

uvec2 lightCellSpans[8];
uint lightCellSpanCount = 0u;
bool lightCellFull = false;

uvec2 lightCellSpan(ivec3 cell) {
  uvec3 key = uvec3(cell) ^ uvec3(0x80000000u);
  uint mask = lightWords[3];
  uint hash = key.x * 0x9e3779b1u ^ key.y * 0x85ebca77u ^ key.z * 0xc2b2ae3du;
  for (uint probe = 0u; probe <= mask; ++probe) {
    uint at = lightWords[2] + 5u * ((hash + probe) & mask);
    if (lightWords[at + 4u] == 0u) { return uvec2(0); }
    if (all(equal(key, uvec3(lightWords[at], lightWords[at + 1u], lightWords[at + 2u])))) {
      return uvec2(lightWords[at + 3u], lightWords[at + 4u]);
    }
  }
  return uvec2(0);
}

bool lightCellBounds(vec3 p, out ivec3 first, out ivec3 last) {
  if (lightFrame.positionCell.w == 2 || any(isnan(p)) || any(isinf(p))) { return false; }
  const float width = 32.0;
  float maxRange = uintBitsToFloat(lightWords[5]);
  float marginM = 16.0 * 1.1920928955078125e-7 *
                  (max(max(abs(p.x), abs(p.y)), abs(p.z)) + maxRange + 128.0);
  if (!(marginM < width * 0.5)) { return false; }
  ivec3 carry = lightFrame.lookupCell.xyz - lightFrame.positionCell.xyz * int(128.0 / width);
  vec3 lookupLocal = lightFrame.positionLocal.xyz - vec3(carry) * width;
  vec3 local = p + lookupLocal;
  vec3 low = floor((local - marginM) * (1.0 / width));
  vec3 high = floor((local + marginM) * (1.0 / width));
  if (any(greaterThanEqual(high, vec3(2147483648.0))) ||
      any(lessThan(low, vec3(-2147483648.0)))) { return false; }
  ivec3 a = ivec3(low), b = ivec3(high);
  for (int axis = 0; axis < 3; ++axis) {
    if (b[axis] > 0 && lightFrame.lookupCell[axis] > 2147483647 - b[axis]) { return false; }
    if (a[axis] < 0 && lightFrame.lookupCell[axis] < (-2147483647 - 1) - a[axis]) { return false; }
  }
  first = lightFrame.lookupCell.xyz + a;
  last = lightFrame.lookupCell.xyz + b;
  return all(lessThanEqual(last - first, ivec3(1)));
}

uint lightCellCount(vec3 p) {
  lightCellSpanCount = 0u;
  ivec3 first, last;
  lightCellFull = !lightCellBounds(p, first, last);
  if (lightCellFull) { return lightWords[6]; }
  uint total = lightWords[1];
  for (int x = 0; x <= last.x - first.x; ++x) {
    for (int y = 0; y <= last.y - first.y; ++y) {
      for (int z = 0; z <= last.z - first.z; ++z) {
        uvec2 span = lightCellSpan(first + ivec3(x, y, z));
        if (span.y == 0u) { continue; }
        if (span.y >= lightWords[6] - total) {
          lightCellFull = true;
          return lightWords[6];
        }
        lightCellSpans[lightCellSpanCount++] = span;
        total += span.y;
      }
    }
  }
  return total;
}

bool lightCellHas(uvec2 span, uint id) {
  uint first = 0u, last = span.y;
  while (first < last) {
    uint mid = first + (last - first) / 2u;
    uint value = lightWords[span.x + mid];
    if (value == id) { return true; }
    if (value < id) { first = mid + 1u; } else { last = mid; }
  }
  return false;
}

uint lightCellId(uint ordinal) {
  if (lightCellFull) { return ordinal; }
  if (ordinal < lightWords[1]) { return lightWords[lightWords[0] + ordinal]; }
  ordinal -= lightWords[1];
  for (uint span = 0u; span < lightCellSpanCount; ++span) {
    if (ordinal < lightCellSpans[span].y) {
      uint id = lightWords[lightCellSpans[span].x + ordinal];
      for (uint earlier = 0u; earlier < span; ++earlier) {
        if (lightCellHas(lightCellSpans[earlier], id)) { return 0xffffffffu; }
      }
      return id;
    }
    ordinal -= lightCellSpans[span].y;
  }
  return 0xffffffffu;
}

Light lightCellEmitter(uint id) {
  Light light = worldEmitters[id];
  if (light.tint.w > 0.5) {
    uint at = lightWords[4] + 3u * id;
    ivec3 cell = ivec3(lightWords[at], lightWords[at + 1u], lightWords[at + 2u]);
    light.place.xyz = fma(vec3(cell - lightFrame.positionCell.xyz), vec3(128.0),
                           light.place.xyz - lightFrame.positionLocal.xyz);
  }
  return light;
}

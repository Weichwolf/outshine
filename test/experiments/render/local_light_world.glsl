#include "local_light_world_position.glsl"
#include "local_light_world_query.glsl"

#ifndef WORLD_SAFE_QUERY
#define WORLD_SAFE_QUERY 1
#endif
#ifndef WORLD_FULL_AT_BOUNDARY
#define WORLD_FULL_AT_BOUNDARY 0
#endif
bool worldFullLookup = false;
uvec2 worldSpans[8];
uint worldSpanCount = 0u;

ivec3 worldCameraCell;
vec3 worldCameraLocal;
vec3 worldRight;
vec3 worldForward;

void worldView() {
  vec3 shift = vec3(0.0);
  float angle = 0.0;
#if WORLD_MOTION
  float phase = probe.control.z == 250.0 ? 1.0 : (probe.control.z == 500.0 ? -1.3 : 0.0);
  shift = clusters.viewZ.xyz * phase;
  angle = clusters.viewZ.w * phase;
#endif
  vec3 localEye = clusters.depth.yzw + shift;
  ivec3 carry = ivec3(floor(localEye / clusters.depth.x));
  worldCameraCell = ivec3(clusters.grid.xyz) + carry;
  worldCameraLocal = localEye - vec3(carry) * clusters.depth.x;
  worldRight = cos(angle) * probe.right.xyz + sin(angle) * probe.forward.xyz;
  worldForward = cos(angle) * probe.forward.xyz - sin(angle) * probe.right.xyz;
}

Light worldLightAt(uint index) {
  Light light = emitters[index];
  if (light.tint.w > 0.5) {
#if WORLD_PRETRANSFORMED
    uint at = (clusters.grid.w >> 1) + index * 3;
    light.place.xyz = uintBitsToFloat(uvec3(words[at], words[at + 1], words[at + 2]));
#else
    uint at = words[4] + index * 3;
    ivec3 cell = ivec3(words[at], words[at + 1], words[at + 2]);
    light.place.xyz = worldRelativePosition(cell, light.place.xyz, worldCameraCell,
                                             worldCameraLocal, clusters.depth.x);
#endif
  }
  return light;
}

bool worldLess(uvec3 a, uvec3 b) {
  return a.x < b.x || (a.x == b.x && (a.y < b.y || (a.y == b.y && a.z < b.z)));
}

uvec2 worldSlot(uint index, uvec3 key) {
  uint at = words[2] + index * 5;
  if (all(equal(key, uvec3(words[at], words[at + 1], words[at + 2])))) {
    return uvec2(words[at + 3], words[at + 4]);
  }
  return uvec2(0);
}

uvec2 worldCellSpan(ivec3 cell) {
  uvec3 key = uvec3(cell) ^ uvec3(0x80000000u);
  uint last = words[3];
  if ((clusters.grid.w & 1u) == 1u) {
    uint first = 0;
    while (first < last) {
      uint mid = (first + last) / 2;
      uint at = words[2] + mid * 5;
      uvec3 other = uvec3(words[at], words[at + 1], words[at + 2]);
      if (all(equal(key, other))) { return worldSlot(mid, key); }
      if (worldLess(other, key)) { first = mid + 1; } else { last = mid; }
    }
    return uvec2(0);
  }
  uint hash = (key.x * 0x9e3779b1u) ^ (key.y * 0x85ebca77u) ^ (key.z * 0xc2b2ae3du);
  for (uint probeAt = 0; probeAt <= last; ++probeAt) {
    uint slot = (hash + probeAt) & last;
    uint at = words[2] + slot * 5;
    if (words[at + 4] == 0u) { return uvec2(0); }
    uvec2 found = worldSlot(slot, key);
    if (found.y != 0u) { return found; }
  }
  return uvec2(0);
}

bool worldSpanContains(uvec2 span, uint id) {
  uint first = 0u, last = span.y;
  while (first < last) {
    uint mid = first + (last - first) / 2u;
    uint value = words[span.x + mid];
    if (value == id) { return true; }
    if (value < id) { first = mid + 1u; } else { last = mid; }
  }
  return false;
}

uint worldIndexAt(uint ordinal) {
  if (worldFullLookup) { return ordinal; }
  if (ordinal < words[1]) { return words[words[0] + ordinal]; }
  ordinal -= words[1];
  for (uint span = 0u; span < worldSpanCount; ++span) {
    if (ordinal < worldSpans[span].y) {
      uint id = words[worldSpans[span].x + ordinal];
      for (uint earlier = 0u; earlier < span; ++earlier) {
        if (worldSpanContains(worldSpans[earlier], id)) { return 0xffffffffu; }
      }
      return id;
    }
    ordinal -= worldSpans[span].y;
  }
  return 0xffffffffu;
}

uvec2 lightSpan(vec3 p) {
  float width = clusters.viewport.x;
#if WORLD_SAFE_QUERY
  ivec3 origin = worldCameraCell * int(clusters.depth.x / width) +
                  ivec3(floor(worldCameraLocal / width));
  ivec3 first, last;
  worldFullLookup = !worldQueryBounds(origin, worldCameraLocal, width, p,
                                       clusters.viewport.y, first, last);
  if (worldFullLookup) { return uvec2(0u, uint(lights.count.x) - words[1]); }
#if WORLD_FULL_AT_BOUNDARY
  if (any(notEqual(first, last))) {
    worldFullLookup = true;
    return uvec2(0u, uint(lights.count.x) - words[1]);
  }
#endif
#else
  ivec3 first = worldCameraCell * int(clusters.depth.x / width) +
                ivec3(floor((p + worldCameraLocal) / width));
  ivec3 last = first;
#endif
#if WORLD_WRONG_CELL
  first.x += 1;
  last.x += 1;
#endif
  uint total = 0u;
  for (int x = 0; x <= last.x - first.x; ++x) {
    for (int y = 0; y <= last.y - first.y; ++y) {
      for (int z = 0; z <= last.z - first.z; ++z) {
        uvec2 span = worldCellSpan(first + ivec3(x, y, z));
        if (span.y == 0u) { continue; }
        worldSpans[worldSpanCount++] = span;
        total += span.y;
      }
    }
  }
  if (total >= uint(lights.count.x) - words[1]) {
    worldFullLookup = true;
    total = uint(lights.count.x) - words[1];
  }
  return uvec2(0u, total);
}

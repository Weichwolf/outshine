#include "local_light_world_position.glsl"

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

uvec2 lightSpan(vec3 p) {
  float lookupWidth = clusters.viewport.x;
  ivec3 cell = worldCameraCell * int(clusters.depth.x / lookupWidth) +
               ivec3(floor((p + worldCameraLocal) / lookupWidth));
#if WORLD_WRONG_CELL
  cell.x += 1;
#endif
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

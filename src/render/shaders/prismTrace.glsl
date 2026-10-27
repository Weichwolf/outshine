#include "prismData.glsl"
#include "prismPlane.glsl"
layout(location = 0) flat in mat4 prismLocalFromClip;
layout(location = 4) flat in uint prismPatchIndex;

float prismInfinity() { return uintBitsToFloat(0x7f800000u); }

bool prismInterval(vec3 origin, vec3 inverseDirection, vec3 low, vec3 high,
                    out float entry, out float leave, out vec3 normal) {
  vec3 a = (low - origin) * inverseDirection;
  vec3 b = (high - origin) * inverseDirection;
  vec3 nearPoint = min(a, b), farPoint = max(a, b);
  bvec3 parallel = isinf(inverseDirection);
  if (any(parallel)) {
    if ((parallel.x && (origin.x < low.x || origin.x > high.x)) ||
        (parallel.y && (origin.y < low.y || origin.y > high.y)) ||
        (parallel.z && (origin.z < low.z || origin.z > high.z))) { return false; }
    nearPoint = mix(nearPoint, vec3(-prismInfinity()), parallel);
    farPoint = mix(farPoint, vec3(prismInfinity()), parallel);
  }
  entry = max(nearPoint.x, max(nearPoint.y, nearPoint.z));
  leave = min(farPoint.x, min(farPoint.y, farPoint.z));
  uint axis = nearPoint.x == entry ? 0u : (nearPoint.y == entry ? 1u : 2u);
  normal = vec3(0.0);
  normal[axis] = inverseDirection[axis] > 0.0 ? -1.0 : 1.0;
  return entry <= leave && leave >= 0.0;
}

bool tracePrism(out vec3 point, out vec3 normal, out float height) {
  PrismPatch gridPatch = prismPatches[prismPatchIndex];
  vec2 pixel = (gl_FragCoord.xy - prismViewport.area.xy) / prismViewport.area.zw;
  vec2 ndc = vec2(pixel.x * 2.0 - 1.0, 1.0 - pixel.y * 2.0);
  vec4 start = prismLocalFromClip * vec4(ndc, 1.0, 1.0);
  vec4 finish = prismLocalFromClip * vec4(ndc, 0.0, 1.0);
  vec3 origin = start.xyz / start.w;
  vec3 ray = finish.xyz - origin * finish.w;
  float squaredLength = dot(ray, ray);
  // Preserve ordinary rays; rescale a squared length that is zero or infinite.
  if (!(squaredLength > 0.0) || isinf(squaredLength)) {
    vec3 magnitude = abs(ray);
    float largest = max(magnitude.x, max(magnitude.y, magnitude.z));
    if (largest == 0.0) { return false; }
    ray /= largest;
  }
  vec3 direction = normalize(ray);
  vec3 inverseDirection = 1.0 / direction;
  float begin, end;
  vec3 unused;
  if (!prismInterval(origin, inverseDirection,
                       vec3(0.0, gridPatch.shape.w, 0.0),
                       vec3(gridPatch.bounds.y, gridPatch.bounds.x, gridPatch.bounds.z), begin, end, unused)) {
    return false;
  }
  vec2 pitch = gridPatch.shape.xy;
  vec2 grid = (origin.xz + max(begin, 0.0) * direction.xz) / pitch;
  ivec2 cell = ivec2(floor(grid));
  if (direction.x < 0.0 && grid.x == floor(grid.x)) { --cell.x; }
  if (direction.z < 0.0 && grid.y == floor(grid.y)) { --cell.y; }
  cell = clamp(cell, ivec2(0), ivec2(gridPatch.samples.yz) - 1);
  ivec2 step = ivec2(direction.x >= 0.0 ? 1 : -1, direction.z >= 0.0 ? 1 : -1);
  vec2 edge = (vec2(cell) + vec2(step.x > 0 ? 1.0 : 0.0, step.y > 0 ? 1.0 : 0.0)) * pitch;
  vec2 next = (edge - origin.xz) * inverseDirection.xz;
  vec2 delta = abs(pitch * inverseDirection.xz);
  if (isinf(inverseDirection.x)) { next.x = prismInfinity(); }
  if (isinf(inverseDirection.z)) { next.y = prismInfinity(); }
  vec2 inset = (1.0 - gridPatch.shape.z) * pitch * 0.5;
  uint most = gridPatch.samples.y + gridPatch.samples.z + 2u;
  for (uint iteration = 0u; iteration < most; ++iteration) {
    uint address = gridPatch.samples.x + uint(cell.y) * gridPatch.samples.y + uint(cell.x);
    vec2 span = prismSamples[address];
    if (span.y > span.x) {
      vec2 low = vec2(cell) * pitch + inset;
      vec2 high = vec2(cell + 1) * pitch - inset;
      float entry, leave;
      if (prismInterval(origin, inverseDirection, vec3(low.x, span.x, low.y),
                           vec3(high.x, span.y, high.y), entry, leave, normal) && entry >= 0.0) {
        vec3 lowFace = vec3(low.x, span.x, low.y);
        vec3 highFace = vec3(high.x, span.y, high.y);
        height = dot(abs(normal), mix(lowFace, highFace, greaterThan(normal, vec3(0.0))));
        float incidence = prismPlaneIncidence(prismLocalFromClip, normal, height);
        if (incidence != 0.0) {
          point = origin + direction * entry;
          return true;
        }
      }
    }
    float crossing = min(next.x, next.y);
    if (crossing > end || isinf(crossing)) { break; }
    if (next.x <= crossing) { cell.x += step.x; next.x += delta.x; }
    if (next.y <= crossing) { cell.y += step.y; next.y += delta.y; }
    if (any(lessThan(cell, ivec2(0))) || any(greaterThanEqual(cell, ivec2(gridPatch.samples.yz)))) {
      break;
    }
  }
  return false;
}

bool tracePrism(out vec3 point, out vec3 normal) {
  float height;
  return tracePrism(point, normal, height);
}

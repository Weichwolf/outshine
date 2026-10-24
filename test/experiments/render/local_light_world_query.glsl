bool worldQueryBounds(ivec3 lookupCell, vec3 eyeLocal, float width, vec3 p,
                       float maxRange, out ivec3 first, out ivec3 last) {
  if (any(isnan(p)) || any(isinf(p))) { return false; }
  float marginM = 16.0 * 1.1920928955078125e-7 *
                  (max(max(abs(p.x), abs(p.y)), abs(p.z)) + maxRange + 128.0);
  if (!(marginM < width * 0.5)) { return false; }
  vec3 local = p + mod(eyeLocal, width);
  vec3 low = floor((local - marginM) / width);
  vec3 high = floor((local + marginM) / width);
  if (any(greaterThanEqual(high, vec3(2147483648.0))) ||
      any(lessThan(low, vec3(-2147483648.0)))) { return false; }
  ivec3 deltaLow = ivec3(low), deltaHigh = ivec3(high);
  for (int axis = 0; axis < 3; ++axis) {
    if (deltaHigh[axis] > 0 && lookupCell[axis] > 2147483647 - deltaHigh[axis]) { return false; }
    if (deltaLow[axis] < 0 && lookupCell[axis] < (-2147483647 - 1) - deltaLow[axis]) { return false; }
  }
  first = lookupCell + deltaLow;
  last = lookupCell + deltaHigh;
  return all(lessThanEqual(last - first, ivec3(1)));
}

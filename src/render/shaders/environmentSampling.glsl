#define ENV_INT(name, value) const int name = value;
#include "render/stages/EnvironmentSpecularValues.inc"
#undef ENV_INT

vec2 octWrap(vec2 uv) {
  if (uv.x < 0.0) { uv = vec2(-uv.x, 1.0 - uv.y); }
  if (uv.x > 1.0) { uv = vec2(2.0 - uv.x, 1.0 - uv.y); }
  if (uv.y < 0.0) { uv = vec2(1.0 - uv.x, -uv.y); }
  if (uv.y > 1.0) { uv = vec2(1.0 - uv.x, 2.0 - uv.y); }
  return uv;
}

vec2 octSigns(vec2 value) {
  return vec2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}

vec3 octDirection(vec2 uv) {
  vec2 p = octWrap(uv) * 2.0 - 1.0;
  vec3 n = vec3(p, 1.0 - abs(p.x) - abs(p.y));
  if (n.z < 0.0) { n.xy = (1.0 - abs(n.yx)) * octSigns(n.xy); }
  return normalize(n);
}

vec2 octCoordinates(vec3 direction) {
  vec3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
  vec2 p = n.xy;
  if (n.z < 0.0) { p = (1.0 - abs(p.yx)) * octSigns(p); }
  return p * 0.5 + 0.5;
}

vec3 environmentHalfVector(vec2 samplePoint, float roughness, vec3 n) {
  float a = roughness * roughness;
  float a2 = a * a;
  float phi = 2.0 * acos(-1.0) * samplePoint.y;
  float cosTheta = sqrt((1.0 - samplePoint.x) / (1.0 + (a2 - 1.0) * samplePoint.x));
  float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
  vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
  vec3 tangent = normalize(cross(up, n));
  return tangent * (cos(phi) * sinTheta) + cross(n, tangent) * (sin(phi) * sinTheta) + n * cosTheta;
}

vec2 environmentUv(vec3 direction, float level) {
  vec2 uv = octCoordinates(direction);
  float span = float(kEnvironmentSide + 2 * kEnvironmentBorder);
  return (vec2(kEnvironmentBorder) + uv * float(kEnvironmentSide) + vec2(0.0, level * span)) /
         vec2(span, span * float(kEnvironmentLevels));
}

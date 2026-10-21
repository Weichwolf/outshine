#version 450
layout(location = 0) noperspective in vec3 barycentric;
layout(location = 1) flat in float farDepth;
layout(location = 2) flat in uint mode;
layout(location = 0) out float depthBound;
void main() {
  vec3 halfFootprint = 0.5 * (abs(dFdx(barycentric)) + abs(dFdy(barycentric)));
  if (any(isnan(barycentric)) || any(isinf(barycentric)) ||
      any(isnan(halfFootprint)) || any(isinf(halfFootprint))) { discard; }
  if (mode != 0u && any(lessThan(barycentric, halfFootprint + vec3(1.0e-4)))) { discard; }
  if (!(farDepth > 0.0)) { discard; }
  depthBound = farDepth;
}

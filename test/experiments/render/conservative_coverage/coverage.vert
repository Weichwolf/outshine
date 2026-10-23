#version 450
layout(std140, set = 1, binding = 0) uniform InputTriangle {
  vec4 points[3];
  uvec4 options;
} inputTriangle;
layout(location = 0) noperspective out vec3 barycentric;
layout(location = 1) flat out float farDepth;
layout(location = 2) flat out uint mode;
void main() {
  bool usable = true;
  farDepth = 1.0;
  for (int i = 0; i < 3; ++i) {
    vec4 p = inputTriangle.points[i];
    usable = usable && !any(isnan(p)) && !any(isinf(p)) && p.w > 1.0e-6;
    usable = usable && p.z >= 0.0 && p.z <= p.w;
    if (p.w > 1.0e-6) { farDepth = min(farDepth, p.z / p.w); }
  }
  gl_Position = usable ? inputTriangle.points[gl_VertexIndex] : vec4(2.0, 0.0, 0.0, 1.0);
  barycentric = vec3(0.0);
  barycentric[gl_VertexIndex] = 1.0;
  farDepth = max(0.0, farDepth - 1.0e-6);
  mode = inputTriangle.options.x;
}

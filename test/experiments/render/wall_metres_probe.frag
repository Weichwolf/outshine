#version 450
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 uv;
layout(location = 1) in vec2 uv1;
layout(location = 10) in vec3 groundWorldM;
layout(location = 11) in float facadeBay;
layout(location = 12) flat in float facadeCode;
layout(std140, set = 3, binding = 0) uniform MetricReference {
  vec4 origin;
  vec4 along;
  vec4 reserved[22];
} reference;
layout(location = 0) out vec4 outUv;
layout(location = 1) out vec4 outMetres;
layout(location = 2) out vec4 outReference;
void main() {
  vec3 relative = groundWorldM - reference.origin.xyz;
  outUv = vec4(uv, facadeCode, 1.0);
  outMetres = vec4(uv1, facadeBay, 1.0);
  outReference = vec4(dot(relative, reference.along.xyz), relative.y, 0.0, 1.0);
}

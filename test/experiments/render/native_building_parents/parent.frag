#version 450
#extension GL_GOOGLE_include_directive : require
const float kPi = 3.14159265358979323846;
#include "brdfTables.glsl"
#include "metalRoughBrdf.glsl"
#include "microfacetEnergy.glsl"
layout(location = 0) in vec3 point;
layout(location = 1) flat in uint primitive;
layout(location = 0) out vec4 colour;
#ifdef PARENT_AUDIT
layout(location = 1) out vec4 audit;
#endif
layout(std140, set = 1, binding = 0) uniform Frame {
  mat4 clip;
  vec4 eye;
  vec4 palette[3];
  uvec4 control;
}

frame;

layout(std430, set = 0, binding = 0) readonly buffer Faces {
  vec4 records[];
};

void main() {
  uint base = primitive * 11u;
  vec4 origin = records[base], gradient = records[base + 1u], normal = records[base + 2u];
  uint upper =
      (gradient.w > 0.5 && origin.w + dot(gradient.xyz, point - origin.xyz) >= 1.0) ? 1u : 0u;
  vec3 albedo = records[base + 3u + upper].xyz;
  vec3 n = normal.xyz, v = normalize(frame.eye.xyz - point), l = normalize(vec3(.45, -.5, .7));
  float nl = dot(n, l), nv = dot(n, v);
  vec3 response = .1 * albedo;
  if (nl > 0 && nv > 0) {
    vec3 h = normalize(l + v);
    float nh = clamp(dot(n, h), 0, 1), vh = max(dot(v, h), 0), aNv = max(nv, 1.e-6);
    for (uint i = 0u; i < 9u; ++i) {
      float weight = records[base + 5u + upper * 3u + i / 4u][i % 4u];
      if (weight == 0) { continue; }
      float r = frame.palette[i / 4u][i % 4u], r2 = r * r;
      Brdf b = metalRoughBrdf(albedo, vec3(.04), 1.0, r2 * r2, nl, aNv, nh, vh);
      const vec2 environmentBrdf = ggxEnvironmentBrdf(aNv, r);
      response += 3.0 * weight * nl *
          (b.diffuse + b.specular * ggxEnergyScaleFromAlbedo(vec3(.04), r, environmentBrdf.x));
    }
  }
  colour = vec4(response, frame.control.y != 0u ? 1.0 : normal.w);
#ifdef PARENT_AUDIT
  audit = vec4(float(primitive + 1u),
               origin.w + dot(gradient.xyz, point - origin.xyz),
               gl_FragCoord.z,
               float(upper));
#endif
}

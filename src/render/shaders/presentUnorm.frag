#version 450

layout(set = 2, binding = 0) uniform sampler2D frame;
layout(location = 0) out vec4 colour;

vec3 srgbFromLinear(vec3 linear) {
  return mix(12.92 * linear, 1.055 * pow(max(linear, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
      greaterThan(linear, vec3(0.0031308)));
}

void main() {
  vec4 linear = texelFetch(frame, ivec2(gl_FragCoord.xy), 0);
  colour = vec4(srgbFromLinear(linear.rgb), linear.a);
}

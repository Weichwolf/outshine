#version 450

layout(set = 2, binding = 0) uniform sampler2D opaque;
layout(set = 2, binding = 1) uniform sampler2D glass;
layout(location = 0) out vec4 colour;

void main() {
  ivec2 px = ivec2(gl_FragCoord.xy);
  vec4 behind = texelFetch(opaque, px, 0);
  vec4 front = texelFetch(glass, px, 0);
  colour = vec4(behind.rgb * (1.0 - front.a) + front.rgb, behind.a);
}

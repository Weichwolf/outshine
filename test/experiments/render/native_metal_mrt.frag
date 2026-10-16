#version 450

layout(location = 0) out vec4 colours[5];
layout(set = 3, binding = 0, std140) uniform UnusedSettings { vec4 value; } unusedSettings;
layout(set = 0, binding = 0, std430) readonly buffer UnusedValues { float values[]; } unusedValues;
layout(set = 2, binding = 0) uniform sampler2D unusedTexture;
layout(set = 2, binding = 1, rgba32f) uniform writeonly image2D unusedImage;

void main() {
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  float depth = float((pixel.x * 7 + pixel.y * 13) % 256) / 256.0;
  for (int i = 0; i < 5; ++i) {
    colours[i] = vec4(depth, float(i) / 4.0, float(pixel.x) / 64.0, 1.0);
  }
  gl_FragDepth = depth;
}

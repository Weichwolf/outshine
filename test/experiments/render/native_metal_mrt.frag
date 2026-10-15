#version 450

layout(location = 0) out vec4 colours[5];

void main() {
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  float depth = float((pixel.x * 7 + pixel.y * 13) % 256) / 256.0;
  for (int i = 0; i < 5; ++i) {
    colours[i] = vec4(depth, float(i) / 4.0, float(pixel.x) / 64.0, 1.0);
  }
  gl_FragDepth = depth;
}

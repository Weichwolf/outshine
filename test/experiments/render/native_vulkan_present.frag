#version 450
layout(location = 0) out vec4 colour;
void main() {
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  colour = vec4(float(pixel.x % 17) / 16.0, float(pixel.y % 13) / 12.0,
                float((pixel.x / 17 + pixel.y / 13) % 2), 1.0);
}

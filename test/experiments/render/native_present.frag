#version 450

layout(location = 0) out vec4 colour;

void main() {
  int band = int(gl_FragCoord.x) / 64 % 3;
  colour = vec4(band == 0 ? 0.8 : 0.1, band == 1 ? 0.8 : 0.1,
                band == 2 ? 0.8 : 0.1, 1.0);
}

vec2 rockCrevice(float value, float variance) {
  float width = 0.07;
  float square = width * width + variance;
  float pulse = width * inversesqrt(square) * exp(-value * value / (2.0 * square));
  return vec2(-pulse, pulse * value / square);
}

vec2 rockCrevice(float value, float footprintM, float wavelengthM) {
  if (wavelengthM <= 0.0) { return vec2(0.0); }
  float visibility = 1.0 - smoothstep(0.25, 0.8, footprintM / wavelengthM);
  if (visibility <= 0.0) { return vec2(0.0); }
  float width = 0.07;
  float across = footprintM / wavelengthM;
  float square = width * width + 1.125 * across * across;
  float pulse = width * inversesqrt(square) * exp(-value * value / (2.0 * square));
  return vec2(-pulse, pulse * value / square) * visibility;
}

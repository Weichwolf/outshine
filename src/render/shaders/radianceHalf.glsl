vec4 halfRounded(vec4 value) {
  uvec4 bits = floatBitsToUint(value);
  uvec4 quantumExponent = max((bits & 0x7fffffffu) >> 23u, uvec4(113u)) - 10u;
  vec4 quantum = uintBitsToFloat(quantumExponent << 23u);
  vec4 inverseQuantum = uintBitsToFloat((uvec4(254u) - quantumExponent) << 23u);
  vec4 rounded = roundEven(value * inverseQuantum) * quantum;
  uvec4 signBits = bits & 0x80000000u;
  uvec4 result = floatBitsToUint(rounded) | signBits;
  return uintBitsToFloat(mix(result, signBits | 0x7f800000u,
                            greaterThanEqual(abs(value), vec4(65520.0))));
}

layout(set = 0, binding = 0, std140) uniform Inputs {
  vec4 arithmetic;
  uvec4 special;
} inputs;

vec4 mathChecks() {
  precise float product = inputs.arithmetic.x * inputs.arithmetic.y + inputs.arithmetic.z;
  float sum = (inputs.arithmetic.w + inputs.arithmetic.x) - inputs.arithmetic.w;
  return vec4(product, sum, float(isnan(uintBitsToFloat(inputs.special.x))),
              float(isinf(uintBitsToFloat(inputs.special.y))));
}

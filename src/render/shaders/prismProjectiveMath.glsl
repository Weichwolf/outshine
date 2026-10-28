#ifndef OUTSHINE_PRISM_PROJECTIVE_MATH
#define OUTSHINE_PRISM_PROJECTIVE_MATH
mat4 prismNormalizeRows(mat4 matrix, out ivec4 exponent) {
  vec4 largest = max(max(abs(matrix[0]), abs(matrix[1])), max(abs(matrix[2]), abs(matrix[3])));
  frexp(largest, exponent);
  for (int i = 0; i < 4; ++i) { matrix[i] = ldexp(matrix[i], -exponent); }
  return matrix;
}

mat4 prismInverseProjection(mat4 matrix) {
  ivec4 rowExponent;
  matrix = prismNormalizeRows(matrix, rowExponent);
  ivec4 columnExponent;
  for (int i = 0; i < 4; ++i) {
    float largest = max(max(abs(matrix[i].x), abs(matrix[i].y)),
                         max(abs(matrix[i].z), abs(matrix[i].w)));
    frexp(largest, columnExponent[i]);
    matrix[i] = ldexp(matrix[i], ivec4(-columnExponent[i]));
  }
  mat4 result = inverse(matrix);
  for (int i = 0; i < 4; ++i) {
    result[i] = ldexp(result[i], -columnExponent - ivec4(rowExponent[i]));
  }
  return result;
}
#endif

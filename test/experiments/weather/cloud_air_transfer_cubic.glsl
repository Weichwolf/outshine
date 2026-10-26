#ifndef OUTSHINE_EXPERIMENT_AIR_TRANSFER_CUBIC_GLSL
#define OUTSHINE_EXPERIMENT_AIR_TRANSFER_CUBIC_GLSL

void transferCubicAxis(int pixel, int length, int extent, int step,
                       out ivec4 indices, out vec4 weights) {
  int cell = min(pixel / step, extent - 2);
  indices = clamp(cell + ivec4(-1, 0, 1, 2), 0, extent - 1);
  vec4 coordinate = vec4(min(indices * step, length - 1));
  float spacing = coordinate.z - coordinate.y;
  float t = (float(pixel) - coordinate.y) / spacing;
  float t2 = t * t;
  float t3 = t2 * t;
  float h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
  float h10 = t3 - 2.0 * t2 + t;
  float h01 = -2.0 * t3 + 3.0 * t2;
  float h11 = t3 - t2;
  float before = spacing / (coordinate.z - coordinate.x);
  float after = spacing / (coordinate.w - coordinate.y);
  weights = vec4(-h10 * before, h00 - h11 * after, h01 + h10 * before, h11 * after);
}

vec3 transferCubicVisibility(sampler2D field, ivec2 pixel, ivec2 imageExtent, int step) {
  ivec2 extent = textureSize(field, 0);
  ivec4 x, y;
  vec4 wx, wy;
  transferCubicAxis(pixel.x, imageExtent.x, extent.x, step, x, wx);
  transferCubicAxis(pixel.y, imageExtent.y, extent.y, step, y, wy);
  vec3 result = vec3(0.0);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      result += wy[row] * wx[column] * texelFetch(field, ivec2(x[column], y[row]), 0).rgb;
    }
  }
  return result;
}

#endif

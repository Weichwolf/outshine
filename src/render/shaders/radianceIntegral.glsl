#include "radiancePair.glsl"
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) uniform sampler2D source;
layout(rgba32f, set = 1, binding = 0) uniform writeonly image2D destination;

void main() {
  ivec2 physical = imageSize(destination);
  ivec2 size = ivec2(physical.x / 2, physical.y);
  int line = int(gl_GlobalInvocationID.x);
  RadiancePair sum = RadiancePair(vec4(0.0), vec4(0.0));
#if INTEGRAL_AXIS == 0
  if (line >= size.y) { return; }
  imageStore(destination, ivec2(0, line), sum.high);
  imageStore(destination, ivec2(1, line), sum.low);
  for (int x = 1; x < size.x; ++x) {
    if (line > 0) {
      sum = radianceAddPair(sum,
                            RadiancePair(texelFetch(source, ivec2(x - 1, line - 1), 0), vec4(0.0)));
    }
    imageStore(destination, ivec2(2 * x, line), sum.high);
    imageStore(destination, ivec2(2 * x + 1, line), sum.low);
  }
#else
  if (line >= size.x) { return; }
  imageStore(destination, ivec2(2 * line, 0), sum.high);
  imageStore(destination, ivec2(2 * line + 1, 0), sum.low);
  for (int y = 1; y < size.y; ++y) {
    sum = radianceAddPair(sum, radianceLoadPair(source, ivec2(line, y)));
    imageStore(destination, ivec2(2 * line, y), sum.high);
    imageStore(destination, ivec2(2 * line + 1, y), sum.low);
  }
#endif
}

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D source;
layout(rgba32f, set = 1, binding = 0) uniform writeonly image2D destination;
void main() {
  ivec2 size = imageSize(destination);
  int line = int(gl_GlobalInvocationID.x);
  vec4 sum = vec4(0.0);
#if INTEGRAL_AXIS == 0
  if (line >= size.y) { return; }
  imageStore(destination, ivec2(0, line), sum);
  for (int x = 1; x < size.x; ++x) {
    if (line > 0) { sum += texelFetch(source, ivec2(x - 1, line - 1), 0); }
    imageStore(destination, ivec2(x, line), sum);
  }
#else
  if (line >= size.x) { return; }
  imageStore(destination, ivec2(line, 0), sum);
  for (int y = 1; y < size.y; ++y) {
    sum += texelFetch(source, ivec2(line, y), 0);
    imageStore(destination, ivec2(line, y), sum);
  }
#endif
}

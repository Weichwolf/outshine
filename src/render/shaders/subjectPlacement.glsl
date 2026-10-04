#ifndef PLACEMENT_BINDING
#define PLACEMENT_BINDING 0
#endif
struct GpuPlacement {
  mat4 current;
  mat4 previous;
  uint colourOffset;
};
layout(std430, set = 0, binding = PLACEMENT_BINDING) readonly buffer Placements {
  GpuPlacement placements[];
};

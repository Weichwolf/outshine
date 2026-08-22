#ifndef KERNELSHAPE_H
#define KERNELSHAPE_H

#include <cstdint>

namespace outshine::Render {

struct ComputeShape {
  uint32_t Samplers = 0;
  uint32_t ReadOnlyTextures = 0;
  uint32_t ReadWriteTextures = 0;
  uint32_t UniformBuffers = 0;
  uint32_t GroupX = 1;
  uint32_t GroupY = 1;
  uint32_t GroupZ = 1;
};

struct DrawShape {
  uint32_t VertexSamplers = 0;
  uint32_t VertexUniformBuffers = 0;
  uint32_t FragmentSamplers = 0;
  uint32_t FragmentUniformBuffers = 0;
};

} // namespace outshine::Render

#endif

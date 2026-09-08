#include "ComputeShaders.h"
#include <iomanip>
#include <iostream>
#include <string>

int main() {
  std::cout << "[\n";
  bool first = true;
  for (const auto &shader : outshine::Render::ComputeShaders()) {
    if (!first) { std::cout << ",\n"; }
    first = false;
    const auto &shape = shader.Shape;
    std::cout << "{\"artifact\":" << std::quoted(std::string(shader.Path)) << ",\"shape\":{"
              << "\"samplers\":" << shape.Samplers
              << ",\"readonly_textures\":" << shape.ReadOnlyTextures
              << ",\"readwrite_textures\":" << shape.ReadWriteTextures
              << ",\"readonly_buffers\":" << shape.ReadOnlyBuffers
              << ",\"readwrite_buffers\":" << shape.ReadWriteBuffers
              << ",\"uniform_buffers\":" << shape.UniformBuffers << ",\"group_x\":" << shape.GroupX
              << ",\"group_y\":" << shape.GroupY << ",\"group_z\":" << shape.GroupZ << "}}";
  }
  std::cout << "\n]\n";
  std::cout.flush();
  return std::cout.good() ? 0 : 1;
}

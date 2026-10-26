#ifndef OUTSHINE_TEST_RENDER_METALSHADER_H
#define OUTSHINE_TEST_RENDER_METALSHADER_H

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

struct MetalBinding {
  uint32_t Set, Binding, Slot, Secondary;
};

struct MetalShader {
  std::string Source, Entry;
  std::vector<MetalBinding> Buffers, Textures;
};

std::expected<MetalShader, std::string> TranslateMetal(const char *path);

#endif

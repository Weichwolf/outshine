#include "MetalShader.h"

#include "src/render/device/native/metal/MetalShader.h"
#include <fstream>

std::expected<MetalShader, std::string> TranslateMetal(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  constexpr std::streamoff maxBytes = 16L * 1024 * 1024;
  constexpr std::streamoff headerBytes = 5 * sizeof(uint32_t);
  constexpr uint32_t magic = 0x07230203u;
  const std::streamoff bytes = input ? input.tellg() : std::streampos(-1);
  if (bytes < headerBytes || bytes > maxBytes || bytes % 4 != 0) {
    return std::unexpected("invalid SPIR-V file size");
  }
  std::vector<uint32_t> words(static_cast<size_t>(bytes) / sizeof(uint32_t));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(words.data()), bytes);
  if (!input || words.front() != magic) { return std::unexpected("unreadable SPIR-V header"); }
  auto shader = outshine::Render::Native::Metal::Translate(words, "main");
  if (!shader) { return std::unexpected(shader.error().Message); }
  MetalShader result{.Source = std::move(shader->Source), .Entry = std::move(shader->Entry)};
  for (const auto &binding : shader->Resources) {
    const MetalBinding mapped{.Set = binding.Source.Set,
                              .Binding = binding.Source.Slot,
                              .Slot = binding.Buffer,
                              .Secondary = binding.Sampler};
    using outshine::Render::Native::BindingKind;
    if (binding.Source.Kind == BindingKind::Uniform ||
        binding.Source.Kind == BindingKind::StorageBuffer) {
      result.Buffers.push_back(mapped);
    } else {
      result.Textures.push_back(mapped);
    }
  }
  return result;
}

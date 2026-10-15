#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_BINDINGS_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_BINDINGS_H

#include "Resources.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

namespace outshine::Render::Native {

enum class BindingKind { SampledTexture, StorageTexture, StorageBuffer, Uniform };
enum class Access { Read, Write, ReadWrite };

struct ShaderBinding {
  uint32_t Set = 0;
  uint32_t Slot = 0;
  BindingKind Kind = BindingKind::StorageBuffer;
  Access Usage = Access::Read;
  uint64_t MinimumBytes = 0;
};

struct SampledTexture {
  TextureView Image;
  const Sampler *Sampling = nullptr;
};

struct StorageTexture {
  TextureView Image;
};

struct StorageBuffer {
  const Buffer *Data = nullptr;
  ByteRange Range;
};

struct Uniform {
  std::span<const std::byte> Bytes;
};

struct Binding {
  uint32_t Set = 0;
  uint32_t Slot = 0;
  std::variant<SampledTexture, StorageTexture, StorageBuffer, Uniform> Value;
};

}
#endif

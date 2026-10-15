#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_COMPUTE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_COMPUTE_H

#include "Resources.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <variant>
#include <vector>

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

struct ComputeLayout {
  std::array<uint32_t, 3> LocalSize = {1, 1, 1};
  std::vector<ShaderBinding> Bindings;
};

class ComputePipeline {
public:
  virtual ~ComputePipeline() = default;
  ComputePipeline(const ComputePipeline &) = delete;
  ComputePipeline &operator=(const ComputePipeline &) = delete;

  [[nodiscard]] const ComputeLayout &Layout() const noexcept { return Layout_; }

protected:
  explicit ComputePipeline(ComputeLayout layout) : Layout_(std::move(layout)) {}

private:
  ComputeLayout Layout_;
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

struct Workgroups {
  uint32_t X = 1;
  uint32_t Y = 1;
  uint32_t Z = 1;
};

}
#endif

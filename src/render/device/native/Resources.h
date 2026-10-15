#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_RESOURCES_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_RESOURCES_H

#include <cstdint>

namespace outshine::Render::Native {

enum class Memory { Device, Upload, Readback };

template <typename Flag> class UseMask {
public:
  constexpr UseMask() = default;

  constexpr UseMask(Flag flag) noexcept : Bits_(static_cast<uint32_t>(flag)) {}

  [[nodiscard]] constexpr UseMask operator|(UseMask right) const noexcept {
    UseMask combined = *this;
    combined.Bits_ |= right.Bits_;
    return combined;
  }

  [[nodiscard]] explicit constexpr operator uint32_t() const noexcept { return Bits_; }

private:
  uint32_t Bits_ = 0;
};

enum class BufferUse : uint32_t {
  None = 0,
  CopySource = 1u << 0u,
  CopyDestination = 1u << 1u,
  Storage = 1u << 2u,
  Vertex = 1u << 3u,
  Index = 1u << 4u,
  Indirect = 1u << 5u
};

[[nodiscard]] constexpr UseMask<BufferUse> operator|(BufferUse left, BufferUse right) noexcept {
  return UseMask(left) | UseMask(right);
}

struct ByteRange {
  uint64_t Offset = 0;
  uint64_t Bytes = 0;
};

struct BufferDescription {
  uint64_t Bytes = 0;
  UseMask<BufferUse> Use;
  Memory Placement = Memory::Device;
};

class Buffer {
public:
  virtual ~Buffer() = default;
  Buffer(const Buffer &) = delete;
  Buffer &operator=(const Buffer &) = delete;

  [[nodiscard]] const BufferDescription &Description() const noexcept { return Description_; }

protected:
  explicit Buffer(BufferDescription description) : Description_(description) {}

private:
  BufferDescription Description_;
};

enum class Format {
  R8Unorm,
  Rgba8Unorm,
  Rgba8Srgb,
  Bgra8Unorm,
  Bgra8Srgb,
  Rg16Float,
  Rgba16Float,
  R32Float,
  Rgba32Float,
  Depth32Float,
  Count
};

enum class TextureUse : uint32_t {
  None = 0,
  CopySource = 1u << 0u,
  CopyDestination = 1u << 1u,
  Sampled = 1u << 2u,
  Storage = 1u << 3u,
  ColourAttachment = 1u << 4u,
  DepthAttachment = 1u << 5u
};

[[nodiscard]] constexpr UseMask<TextureUse> operator|(TextureUse left, TextureUse right) noexcept {
  return UseMask(left) | UseMask(right);
}

enum class TextureType { Image2D, Image2DArray };

struct TextureDescription {
  uint32_t Width = 0;
  uint32_t Height = 0;
  uint32_t Mips = 1;
  uint32_t Layers = 1;
  TextureType Type = TextureType::Image2D;
  Format Texels = Format::Rgba8Unorm;
  UseMask<TextureUse> Use;
};

class Texture {
public:
  virtual ~Texture() = default;
  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;

  [[nodiscard]] const TextureDescription &Description() const noexcept { return Description_; }

protected:
  explicit Texture(TextureDescription description) : Description_(description) {}

private:
  TextureDescription Description_;
};

struct TextureView {
  const Texture *Image = nullptr;
  uint32_t FirstMip = 0;
  uint32_t Mips = 1;
  uint32_t FirstLayer = 0;
  uint32_t Layers = 1;
};

enum class Filter { Nearest, Linear };
enum class Address { Repeat, MirrorRepeat, ClampToEdge };

struct SamplerDescription {
  Filter Min = Filter::Linear;
  Filter Mag = Filter::Linear;
  Filter Mip = Filter::Linear;
  Address U = Address::ClampToEdge;
  Address V = Address::ClampToEdge;
};

class Sampler {
public:
  virtual ~Sampler() = default;
  Sampler(const Sampler &) = delete;
  Sampler &operator=(const Sampler &) = delete;

protected:
  Sampler() = default;
};

}
#endif

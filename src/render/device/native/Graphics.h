#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_GRAPHICS_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_GRAPHICS_H

#include "Bindings.h"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Render::Native {

enum class ShaderStage : uint32_t { Vertex = 1u << 0u, Fragment = 1u << 1u };

[[nodiscard]] constexpr UseMask<ShaderStage> operator|(ShaderStage left,
                                                       ShaderStage right) noexcept {
  return UseMask(left) | UseMask(right);
}

struct GraphicsBinding {
  ShaderBinding Resource;
  UseMask<ShaderStage> Stages;
};

struct GraphicsLayout {
  std::vector<GraphicsBinding> Bindings;
};

struct ShaderCode {
  std::span<const uint32_t> Spirv;
  std::string_view Entry = "main";
};

enum class VertexFormat { Float2, Float3, Float4, Uint32 };
enum class VertexRate { Vertex, Instance };

struct VertexStream {
  uint32_t Slot = 0;
  uint32_t Stride = 0;
  VertexRate Rate = VertexRate::Vertex;
};

struct VertexAttribute {
  uint32_t Location = 0;
  uint32_t Stream = 0;
  uint32_t Offset = 0;
  VertexFormat Format = VertexFormat::Float3;
};

enum class ColourChannel : uint32_t { Red = 1u, Green = 2u, Blue = 4u, Alpha = 8u };

[[nodiscard]] constexpr UseMask<ColourChannel> operator|(ColourChannel left,
                                                         ColourChannel right) noexcept {
  return UseMask(left) | UseMask(right);
}

enum class BlendFactor { Zero, One, SourceAlpha, OneMinusSourceAlpha };

struct BlendFunction {
  BlendFactor Source = BlendFactor::One;
  BlendFactor Destination = BlendFactor::Zero;
};

struct ColourTarget {
  Format Texels = Format::Rgba8Unorm;
  UseMask<ColourChannel> Write =
      ColourChannel::Red | ColourChannel::Green | ColourChannel::Blue | ColourChannel::Alpha;
  bool Blend = false;
  BlendFunction Rgb;
  BlendFunction Alpha;
};

enum class DepthCompare { Greater, GreaterEqual };

struct DepthTarget {
  Format Texels = Format::Depth32Float;
  DepthCompare Compare = DepthCompare::Greater;
  bool Test = true;
  bool Write = true;
};

enum class Cull { None, Front, Back };
enum class Winding { Clockwise, CounterClockwise };

struct GraphicsDescription {
  ShaderCode Vertex;
  std::optional<ShaderCode> Fragment;
  std::span<const VertexStream> Streams;
  std::span<const VertexAttribute> Attributes;
  std::span<const ColourTarget> Colours;
  std::optional<DepthTarget> Depth;
  Cull Culling = Cull::Back;
  Winding Front = Winding::CounterClockwise;
  bool ClampDepth = false;
  std::string_view Label;
};

class GraphicsPipeline {
public:
  virtual ~GraphicsPipeline() = default;
  GraphicsPipeline(const GraphicsPipeline &) = delete;
  GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;

  [[nodiscard]] const GraphicsLayout &Layout() const noexcept { return Layout_; }

protected:
  explicit GraphicsPipeline(GraphicsLayout layout) : Layout_(std::move(layout)) {}

private:
  GraphicsLayout Layout_;
};

}
#endif

#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_RENDER_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_RENDER_H

#include "Graphics.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace outshine::Render::Native {

enum class Load { Load, Clear, Discard };
enum class Store { Store, Discard };

struct ColourAttachment {
  TextureView Image;
  Load Begin = Load::Load;
  Store End = Store::Store;
  std::array<float, 4> Clear{};
};

struct DepthAttachment {
  TextureView Image;
  Load Begin = Load::Load;
  Store End = Store::Store;
  float Clear = 0;
};

struct RenderPassDescription {
  std::span<const ColourAttachment> Colours;
  std::optional<DepthAttachment> Depth;
};

struct Viewport {
  float X = 0;
  float Y = 0;
  float Width = 0;
  float Height = 0;
  float MinDepth = 0;
  float MaxDepth = 1;
};

struct Scissor {
  uint32_t X = 0;
  uint32_t Y = 0;
  uint32_t Width = 0;
  uint32_t Height = 0;
};

struct VertexBuffer {
  uint32_t Stream = 0;
  const Buffer *Data = nullptr;
  ByteRange Range;
};

struct DrawState {
  const GraphicsPipeline *Pipeline = nullptr;
  std::span<const Binding> Bindings;
  std::span<const VertexBuffer> Vertices;
  Viewport Area;
  Scissor Clip;
};

struct VertexRange {
  uint32_t First = 0;
  uint32_t Count = 0;
  uint32_t Instances = 1;
  uint32_t FirstInstance = 0;
};

struct DrawDescription {
  DrawState State;
  VertexRange Range;
};

enum class IndexType { Uint16, Uint32 };

struct IndexBuffer {
  const Buffer *Data = nullptr;
  ByteRange Range;
  IndexType Type = IndexType::Uint32;
};

struct IndexRange {
  uint32_t First = 0;
  uint32_t Count = 0;
  uint32_t Instances = 1;
  int32_t BaseVertex = 0;
  uint32_t FirstInstance = 0;
};

struct IndexedDrawDescription {
  DrawState State;
  IndexBuffer Indices;
  IndexRange Range;
};

struct IndexedIndirectArguments {
  uint32_t IndexCount = 0;
  uint32_t InstanceCount = 0;
  uint32_t FirstIndex = 0;
  int32_t BaseVertex = 0;
  uint32_t FirstInstance = 0;
};

inline constexpr uint32_t kIndexedIndirectBytes = 20;

static_assert(sizeof(IndexedIndirectArguments) == kIndexedIndirectBytes);
static_assert(alignof(IndexedIndirectArguments) == alignof(uint32_t));
static_assert(offsetof(IndexedIndirectArguments, IndexCount) == 0);
static_assert(offsetof(IndexedIndirectArguments, InstanceCount) == 4);
static_assert(offsetof(IndexedIndirectArguments, FirstIndex) == 8);
static_assert(offsetof(IndexedIndirectArguments, BaseVertex) == 12);
static_assert(offsetof(IndexedIndirectArguments, FirstInstance) == 16);

struct IndirectRange {
  const Buffer *Data = nullptr;
  uint64_t Offset = 0;
  uint32_t Count = 0;
  uint32_t Stride = sizeof(IndexedIndirectArguments);
};

struct IndirectDrawDescription {
  DrawState State;
  IndexBuffer Indices;
  IndirectRange Range;
};

}
#endif

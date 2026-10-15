#include "MetalInternal.h"
#include <array>
#include <cstddef>

namespace outshine::Render::Native::Metal {
namespace {

static_assert(sizeof(MTLDrawIndexedPrimitivesIndirectArguments) ==
              sizeof(IndexedIndirectArguments));
static_assert(alignof(MTLDrawIndexedPrimitivesIndirectArguments) ==
              alignof(IndexedIndirectArguments));
static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexCount) ==
              offsetof(IndexedIndirectArguments, IndexCount));
static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, instanceCount) ==
              offsetof(IndexedIndirectArguments, InstanceCount));
static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexStart) ==
              offsetof(IndexedIndirectArguments, FirstIndex));
static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseVertex) ==
              offsetof(IndexedIndirectArguments, BaseVertex));
static_assert(offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseInstance) ==
              offsetof(IndexedIndirectArguments, FirstInstance));

struct IndexArgument {
  std::shared_ptr<BufferState> State;
  MTLIndexType Type;
  uint32_t Unit;
  ByteRange Range;
};

Result<IndexArgument> Indices(const IndexBuffer &binding, uint64_t device) {
  const auto *const buffer = dynamic_cast<const MetalBuffer *>(binding.Data);
  const uint32_t unit = binding.Type == IndexType::Uint16 ? 2 : 4;
  if ((binding.Type != IndexType::Uint16 && binding.Type != IndexType::Uint32) ||
      buffer == nullptr || buffer->State->Device != device ||
      !Has(buffer->Description().Use, BufferUse::Index) ||
      !Fits(binding.Range, buffer->Description().Bytes) || binding.Range.Offset % unit != 0 ||
      binding.Range.Bytes % unit != 0) {
    return Failure(ErrorKind::Invalid, "invalid Metal index buffer, range, alignment or ownership");
  }
  return IndexArgument{.State = buffer->State,
                       .Type = binding.Type == IndexType::Uint16 ? MTLIndexTypeUInt16
                                                                 : MTLIndexTypeUInt32,
                       .Unit = unit,
                       .Range = binding.Range};
}

Result<std::shared_ptr<BufferState>> IndirectArguments(IndirectRange range, uint64_t device) {
  const auto *const buffer = dynamic_cast<const MetalBuffer *>(range.Data);
  const uint64_t bytes =
      range.Count == 0 ? 0 : uint64_t{range.Count - 1} * range.Stride + kIndexedIndirectBytes;
  if (buffer == nullptr || buffer->State->Device != device ||
      !Has(buffer->Description().Use, BufferUse::Indirect) || range.Offset % 4 != 0 ||
      range.Stride < kIndexedIndirectBytes || range.Stride % 4 != 0 ||
      range.Offset > buffer->Description().Bytes ||
      bytes > buffer->Description().Bytes - range.Offset) {
    return Failure(ErrorKind::Invalid, "invalid Metal indirect buffer, stride, range or ownership");
  }
  return buffer->State;
}

}

Result<void> MetalCommands::DrawIndexed(const IndexedDrawDescription &description) {
  const auto index = Indices(description.Indices, Device);
  if (!index) { return std::unexpected(index.error()); }
  const auto range = description.Range;
  const uint64_t first = uint64_t{range.First} * index->Unit;
  const uint64_t bytes = uint64_t{range.Count} * index->Unit;
  if (first > index->Range.Bytes || bytes > index->Range.Bytes - first) {
    return Failure(ErrorKind::Invalid, "Metal draw exceeds the bound index span");
  }
  const std::array reads{index->State};
  const auto prepared = PrepareDraw(
      description.State,
      {.Count = range.Count, .Instances = range.Instances, .FirstInstance = range.FirstInstance},
      DrawKind::Indexed,
      reads);
  if (!prepared) { return prepared; }
  if (range.Count == 0 || range.Instances == 0) { return {}; }
  [Render->Encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                              indexCount:range.Count
                               indexType:index->Type
                             indexBuffer:index->State->Native
                       indexBufferOffset:index->Range.Offset + first
                           instanceCount:range.Instances
                              baseVertex:range.BaseVertex
                            baseInstance:range.FirstInstance];
  return {};
}

Result<void> MetalCommands::DrawIndexedIndirect(const IndirectDrawDescription &description) {
  const auto index = Indices(description.Indices, Device);
  if (!index) { return std::unexpected(index.error()); }
  const auto arguments = IndirectArguments(description.Range, Device);
  if (!arguments) { return std::unexpected(arguments.error()); }
  const std::array reads{index->State, *arguments};
  const auto prepared = PrepareDraw(description.State, {}, DrawKind::Indirect, reads);
  if (!prepared) { return prepared; }
  const auto range = description.Range;
  for (uint32_t draw = 0; draw < range.Count; ++draw) {
    [Render->Encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                 indexType:index->Type
                               indexBuffer:index->State->Native
                         indexBufferOffset:index->Range.Offset
                            indirectBuffer:(*arguments)->Native
                      indirectBufferOffset:range.Offset + uint64_t{draw} * range.Stride];
  }
  return {};
}

}

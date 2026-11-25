#include "MetalInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

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

bool FitsIndices(const IndexArgument &index, IndexRange range) {
  const uint64_t first = uint64_t{range.First} * index.Unit;
  const uint64_t bytes = uint64_t{range.Count} * index.Unit;
  return first <= index.Range.Bytes && bytes <= index.Range.Bytes - first;
}

void EncodeIndexed(id<MTLRenderCommandEncoder> encoder,
                   const IndexArgument &index,
                   IndexRange range) {
  if (range.Count == 0 || range.Instances == 0) { return; }
  [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                      indexCount:range.Count
                       indexType:index.Type
                     indexBuffer:index.State->Native
               indexBufferOffset:index.Range.Offset + uint64_t{range.First} * index.Unit
                   instanceCount:range.Instances
                      baseVertex:range.BaseVertex
                    baseInstance:range.FirstInstance];
}

void EncodeIndirect(id<MTLRenderCommandEncoder> encoder,
                    const IndexArgument &index,
                    const BufferState &arguments,
                    uint64_t at) {
  [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                       indexType:index.Type
                     indexBuffer:index.State->Native
               indexBufferOffset:index.Range.Offset
                  indirectBuffer:arguments.Native
            indirectBufferOffset:at];
}

}

Result<void> MetalCommands::DrawIndexed(const IndexedDrawDescription &description) {
  const auto index = Indices(description.Indices, Device);
  if (!index) { return std::unexpected(index.error()); }
  const auto range = description.Range;
  if (!FitsIndices(*index, range)) {
    return Failure(ErrorKind::Invalid, "Metal draw exceeds the bound index span");
  }
  const std::array reads{index->State};
  const auto prepared = PrepareDraw(
      description.State,
      {.Count = range.Count, .Instances = range.Instances, .FirstInstance = range.FirstInstance},
      DrawKind::Indexed,
      reads);
  if (!prepared) { return prepared; }
  EncodeIndexed(Render->Encoder, *index, range);
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
    EncodeIndirect(
        Render->Encoder, *index, **arguments, range.Offset + uint64_t{draw} * range.Stride);
  }
  return {};
}

Result<void> MetalCommands::DrawIndexedSequence(const Native::IndexedDrawSequence &sequence) {
  if (sequence.SourceRanges.size() != sequence.Arguments.Count) {
    return Failure(ErrorKind::Invalid, "indexed draw sequence has mismatched rows");
  }
  if (sequence.SourceRanges.empty()) { return {}; }
  const auto source = Indices(sequence.SourceIndices, Device);
  if (!source) { return std::unexpected(source.error()); }
  const auto compacted = Indices(sequence.CompactedIndices, Device);
  if (!compacted) { return std::unexpected(compacted.error()); }
  const auto args = IndirectArguments(sequence.Arguments, Device);
  if (!args) { return std::unexpected(args.error()); }
  uint64_t instanceEnd = 0;
  for (const auto &range : sequence.SourceRanges) {
    if (!range) { continue; }
    if (!FitsIndices(*source, *range)) {
      return Failure(ErrorKind::Invalid, "Metal draw exceeds the bound index span");
    }
    if (range->Count != 0 && range->Instances != 0) {
      instanceEnd = std::max(instanceEnd, uint64_t{range->FirstInstance} + range->Instances);
    }
  }
  if (instanceEnd > uint64_t{std::numeric_limits<uint32_t>::max()} + 1u) {
    return Failure(ErrorKind::Invalid, "Metal draw exceeds the instance addressing range");
  }
  const VertexRange instanceSpan =
      instanceEnd == 0
          ? VertexRange{}
          : VertexRange{.Count = 1, .FirstInstance = static_cast<uint32_t>(instanceEnd - 1)};
  const std::array reads{source->State, compacted->State, *args};
  const auto prepared = PrepareDraw(sequence.State,
                                    instanceSpan,
                                    instanceEnd == 0 ? DrawKind::Indirect : DrawKind::Indexed,
                                    reads);
  if (!prepared) { return prepared; }
  const auto &arguments = sequence.Arguments;
  for (size_t at = 0; at < sequence.SourceRanges.size(); ++at) {
    const auto &range = sequence.SourceRanges[at];
    if (range) {
      EncodeIndexed(Render->Encoder, *source, *range);
    } else {
      EncodeIndirect(Render->Encoder, *compacted, **args, arguments.Offset + at * arguments.Stride);
    }
  }
  return {};
}

}

#ifdef __linux__
#include "Recording.h"
#include <expected>
#include <cstdint>
#include <cstddef>
#include <utility>

namespace outshine::Render::Native::Vulkan {

static_assert(sizeof(VkDrawIndexedIndirectCommand) == sizeof(IndexedIndirectArguments));
static_assert(alignof(VkDrawIndexedIndirectCommand) == alignof(IndexedIndirectArguments));
static_assert(offsetof(VkDrawIndexedIndirectCommand, indexCount) ==
              offsetof(IndexedIndirectArguments, IndexCount));
static_assert(offsetof(VkDrawIndexedIndirectCommand, instanceCount) ==
              offsetof(IndexedIndirectArguments, InstanceCount));
static_assert(offsetof(VkDrawIndexedIndirectCommand, firstIndex) ==
              offsetof(IndexedIndirectArguments, FirstIndex));
static_assert(offsetof(VkDrawIndexedIndirectCommand, vertexOffset) ==
              offsetof(IndexedIndirectArguments, BaseVertex));
static_assert(offsetof(VkDrawIndexedIndirectCommand, firstInstance) ==
              offsetof(IndexedIndirectArguments, FirstInstance));

Result<void> Recording::AppendDraw(DrawPacket packet) {
  if (!Render_) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = "no open render pass"});
  }
  Render_->Draws.push_back(std::move(packet));
  return {};
}

Result<void> Recording::Draw(const DrawDescription &description) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  auto packet = PrepareDraw(description.State);
  if (!packet) { return std::unexpected(packet.error()); }
  if (auto range = CheckVertexRange(*packet->Graphics, packet->Vertices, description.Range, false);
      !range) {
    return range;
  }
  packet->Direct = description.Range;
  return AppendDraw(std::move(*packet));
}

Result<void> Recording::DrawIndexed(const IndexedDrawDescription &description) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  auto packet = PrepareDraw(description.State);
  if (!packet) { return std::unexpected(packet.error()); }
  if (auto indices = BindIndices(*packet, description.Indices); !indices) { return indices; }
  const uint32_t bytes = description.Indices.Type == IndexType::Uint16 ? 2u : 4u;
  const auto &range = description.Range;
  if (uint64_t(range.First) + range.Count > description.Indices.Range.Bytes / bytes) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "draw exceeds bound index range"});
  }
  if (auto vertices =
          CheckVertexRange(*packet->Graphics,
                           packet->Vertices,
                           {.Instances = range.Instances, .FirstInstance = range.FirstInstance},
                           true);
      !vertices) {
    return vertices;
  }
  packet->Indexed = range;
  return AppendDraw(std::move(*packet));
}

Result<void> Recording::DrawIndexedIndirect(const IndirectDrawDescription &description) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  const auto &range = description.Range;
  if (range.Data == nullptr || range.Offset % 4 != 0 || range.Stride < kIndexedIndirectBytes ||
      range.Stride % 4 != 0 || range.Count == 0 ||
      range.Count > Context_->Properties.limits.maxDrawIndirectCount) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid indexed indirect range or stride"});
  }
  if (!Context_->IndirectFirstInstance) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "nonzero indirect first instance unavailable"});
  }
  auto buffer = BufferFrom(*range.Data, *Context_);
  if (!buffer) { return std::unexpected(buffer.error()); }
  const ByteRange bytes{.Offset = range.Offset,
                        .Bytes = uint64_t(range.Count - 1) * range.Stride + kIndexedIndirectBytes};
  if (!Contains((*buffer)->Description.Bytes, bytes) ||
      (static_cast<uint32_t>((*buffer)->Description.Use) &
       static_cast<uint32_t>(BufferUse::Indirect)) == 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid indirect buffer usage or bounds"});
  }
  auto packet = PrepareDraw(description.State);
  if (!packet) { return std::unexpected(packet.error()); }
  if (auto indices = BindIndices(*packet, description.Indices); !indices) { return indices; }
  if (auto held = Hold(*buffer); !held) { return held; }
  Use(**buffer,
      bytes,
      {.Stage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
       .Access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});
  packet->Indirect = std::move(*buffer);
  packet->IndirectBytes = range;
  packet->IndirectBytes.Data = nullptr;
  return AppendDraw(std::move(*packet));
}

void Recording::EncodeDraw(const DrawPacket &packet) {
  const auto &pipeline = *packet.Graphics->Pipeline;
  vkCmdSetViewport(Commands_, 0, 1, &packet.Area);
  vkCmdSetScissor(Commands_, 0, 1, &packet.Clip);
  if (!packet.Sets.empty()) {
    vkCmdBindDescriptorSets(Commands_,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.Layout,
                            0,
                            static_cast<uint32_t>(packet.Sets.size()),
                            packet.Sets.data(),
                            0,
                            nullptr);
  }
  for (const auto &vertex : packet.Vertices) {
    vkCmdBindVertexBuffers2(Commands_,
                            vertex.Stream.Slot,
                            1,
                            &vertex.Buffer->Handle,
                            &vertex.Range.Offset,
                            &vertex.Range.Bytes,
                            nullptr);
  }
  if (!packet.Indices) {
    vkCmdDraw(Commands_,
              packet.Direct.Count,
              packet.Direct.Instances,
              packet.Direct.First,
              packet.Direct.FirstInstance);
    return;
  }
  vkCmdBindIndexBuffer(
      Commands_, packet.Indices->Handle, packet.IndexBytes.Offset, packet.IndexFormat);
  if (!packet.Indirect) {
    vkCmdDrawIndexed(Commands_,
                     packet.Indexed.Count,
                     packet.Indexed.Instances,
                     packet.Indexed.First,
                     packet.Indexed.BaseVertex,
                     packet.Indexed.FirstInstance);
    return;
  }
  const auto &range = packet.IndirectBytes;
  if (Context_->MultiDrawIndirect) {
    vkCmdDrawIndexedIndirect(
        Commands_, packet.Indirect->Handle, range.Offset, range.Count, range.Stride);
  } else {
    for (uint32_t at = 0; at < range.Count; ++at) {
      vkCmdDrawIndexedIndirect(Commands_,
                               packet.Indirect->Handle,
                               range.Offset + uint64_t(at) * range.Stride,
                               1,
                               range.Stride);
    }
  }
}

}
#endif

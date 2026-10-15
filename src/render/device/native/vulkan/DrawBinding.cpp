#ifdef __linux__
#include "Recording.h"
#include <algorithm>
#include <expected>
#include <variant>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace outshine::Render::Native::Vulkan {

Result<void> Recording::CheckFeedback(std::span<const Binding> bindings) const {
  if (!Render_) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = "no open render pass"});
  }
  for (const auto &binding : bindings) {
    TextureView view;
    if (const auto *sampled = std::get_if<SampledTexture>(&binding.Value)) {
      view = sampled->Image;
    }
    if (const auto *storage = std::get_if<StorageTexture>(&binding.Value)) {
      view = storage->Image;
    }
    if (view.Image == nullptr) { continue; }
    const auto image = ImageFrom(*view.Image, *Context_);
    if (!image) { return std::unexpected(image.error()); }
    const auto aliases = [&](const RenderAttachment &attachment) {
      return attachment.Image == *image && Overlaps(attachment.Range, view);
    };
    if (std::ranges::any_of(Render_->Colours, aliases) ||
        (Render_->Depth && aliases(*Render_->Depth))) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "shader binding overlaps a render attachment"});
    }
  }
  return {};
}

Result<void> Recording::BindVertices(DrawPacket &packet, std::span<const VertexBuffer> vertices) {
  if (vertices.size() != packet.Graphics->Streams.size()) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "vertex buffer count does not match graphics streams"});
  }
  for (const auto &stream : packet.Graphics->Streams) {
    const auto found = std::ranges::find(vertices, stream.Slot, &VertexBuffer::Stream);
    if (found == vertices.end() || found->Data == nullptr) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "vertex stream is not bound"});
    }
    auto buffer = BufferFrom(*found->Data, *Context_);
    if (!buffer) { return std::unexpected(buffer.error()); }
    if (!Contains((*buffer)->Description.Bytes, found->Range) ||
        (static_cast<uint32_t>((*buffer)->Description.Use) &
         static_cast<uint32_t>(BufferUse::Vertex)) == 0) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "invalid vertex buffer usage or byte range"});
    }
    for (const auto &attribute : packet.Graphics->Attributes) {
      if (attribute.Stream == stream.Slot &&
          (found->Range.Offset + attribute.Offset) % sizeof(float) != 0) {
        return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                     .Message = "misaligned vertex attribute buffer offset"});
      }
    }
    if (auto held = Hold(*buffer); !held) { return std::unexpected(held.error()); }
    Use(**buffer,
        found->Range,
        {.Stage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
         .Access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT});
    packet.Vertices.push_back(
        {.Stream = stream, .Buffer = std::move(*buffer), .Range = found->Range});
  }
  return {};
}

Result<DrawPacket> Recording::PrepareDraw(const DrawState &description) {
  const auto *const pipeline = dynamic_cast<const VulkanGraphicsPipeline *>(description.Pipeline);
  if (!Render_ || Finished_ || pipeline == nullptr ||
      pipeline->State()->Pipeline->Owner != Context_) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid,
              .Message = "draw requires an open pass and owned graphics pipeline"});
  }
  constexpr std::size_t maxDrawPackets = 16384;
  if (Render_->Draws.size() >= maxDrawPackets) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Busy, .Message = "per-pass draw packet budget exhausted"});
  }
  const auto &graphics = *pipeline->State();
  if (graphics.Colours.size() != Render_->Colours.size() ||
      graphics.Depth.has_value() != Render_->Depth.has_value()) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "graphics targets do not match the pass"});
  }
  for (std::size_t at = 0; at < graphics.Colours.size(); ++at) {
    if (graphics.Colours[at].Texels != Render_->Colours[at].Image->Description.Texels) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "graphics colour format does not match the pass"});
    }
  }
  if (auto area = CheckDrawArea(*Context_, *Render_, description); !area) {
    return std::unexpected(area.error());
  }
  if (auto feedback = CheckFeedback(description.Bindings); !feedback) {
    return std::unexpected(feedback.error());
  }
  DrawPacket packet;
  packet.Graphics = pipeline->State();
  if (auto vertices = BindVertices(packet, description.Vertices); !vertices) {
    return std::unexpected(vertices.error());
  }
  auto sets = BindResources(*graphics.Pipeline,
                            graphics.Bindings,
                            description.Bindings,
                            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
  if (!sets) { return std::unexpected(sets.error()); }
  packet.Sets = std::move(*sets);
  packet.Area = {.x = description.Area.X,
                 .y = description.Area.Y + description.Area.Height,
                 .width = description.Area.Width,
                 .height = -description.Area.Height,
                 .minDepth = description.Area.MinDepth,
                 .maxDepth = description.Area.MaxDepth};
  packet.Clip = {.offset = {.x = static_cast<int32_t>(description.Clip.X),
                            .y = static_cast<int32_t>(description.Clip.Y)},
                 .extent = {.width = description.Clip.Width, .height = description.Clip.Height}};
  if (std::ranges::find(Pipelines_, graphics.Pipeline) == Pipelines_.end()) {
    Pipelines_.push_back(graphics.Pipeline);
  }
  return packet;
}

Result<void> Recording::BindIndices(DrawPacket &packet, const IndexBuffer &indices) {
  if (indices.Data == nullptr ||
      (indices.Type != IndexType::Uint16 && indices.Type != IndexType::Uint32)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid index buffer type"});
  }
  auto buffer = BufferFrom(*indices.Data, *Context_);
  if (!buffer) { return std::unexpected(buffer.error()); }
  const uint32_t bytes = indices.Type == IndexType::Uint16 ? 2u : 4u;
  if (!Contains((*buffer)->Description.Bytes, indices.Range) || indices.Range.Offset % bytes != 0 ||
      indices.Range.Bytes % bytes != 0 ||
      (static_cast<uint32_t>((*buffer)->Description.Use) &
       static_cast<uint32_t>(BufferUse::Index)) == 0) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "invalid index buffer usage, range or alignment"});
  }
  if (auto held = Hold(*buffer); !held) { return held; }
  Use(**buffer,
      indices.Range,
      {.Stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, .Access = VK_ACCESS_2_INDEX_READ_BIT});
  packet.Indices = std::move(*buffer);
  packet.IndexBytes = indices.Range;
  packet.IndexFormat =
      indices.Type == IndexType::Uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
  return {};
}

}
#endif

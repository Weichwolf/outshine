#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_RENDERSTATE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_RENDERSTATE_H

#ifdef __linux__
#include "Buffer.h"
#include "VulkanGraphics.h"
#include "VulkanImage.h"
#include <optional>

namespace outshine::Render::Native::Vulkan {

struct RenderAttachment {
  std::shared_ptr<ImageState> Image;
  TextureView Range;
  VkRenderingAttachmentInfo Native{};
};

struct BoundVertex {
  VertexStream Stream;
  std::shared_ptr<BufferState> Buffer;
  ByteRange Range;
};

struct DrawPacket {
  std::shared_ptr<GraphicsState> Graphics;
  std::vector<VkDescriptorSet> Sets;
  std::vector<BoundVertex> Vertices;
  VkViewport Area{};
  VkRect2D Clip{};
  VertexRange Direct;
  std::shared_ptr<BufferState> Indices;
  ByteRange IndexBytes;
  VkIndexType IndexFormat = VK_INDEX_TYPE_UINT32;
  IndexRange Indexed;
  std::shared_ptr<BufferState> Indirect;
  IndirectRange IndirectBytes;
};

struct RenderState {
  uint32_t Width = 0;
  uint32_t Height = 0;
  std::vector<RenderAttachment> Colours;
  std::optional<RenderAttachment> Depth;
  std::vector<DrawPacket> Draws;
};

[[nodiscard]] bool Overlaps(const TextureView &left, const TextureView &right) noexcept;
[[nodiscard]] Result<void>
CheckDrawArea(const Context &context, const RenderState &pass, const DrawState &draw);
[[nodiscard]] Result<void> CheckVertexRange(const GraphicsState &graphics,
                                            std::span<const BoundVertex> vertices,
                                            VertexRange range,
                                            bool indexed);

}
#endif
#endif

#ifdef __linux__
#include "VulkanGraphics.h"
#include "VulkanImage.h"
#include <algorithm>
#include <expected>
#include <cstddef>
#include <cstdint>

namespace outshine::Render::Native::Vulkan {

VkFormat AttributeFormat(VertexFormat format) noexcept {
  switch (format) {
    case VertexFormat::Float2: return VK_FORMAT_R32G32_SFLOAT;
    case VertexFormat::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
    case VertexFormat::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case VertexFormat::Uint32: return VK_FORMAT_R32_UINT;
  }
  return VK_FORMAT_UNDEFINED;
}

uint32_t AttributeBytes(VertexFormat format) noexcept {
  switch (format) {
    case VertexFormat::Float2: return 8;
    case VertexFormat::Float3: return 12;
    case VertexFormat::Float4: return 16;
    case VertexFormat::Uint32: return 4;
  }
  return 0;
}

namespace {

Result<void> CheckStreams(const Context &context, const GraphicsDescription &description) {
  const auto &limits = context.Properties.limits;
  if (description.Streams.size() > limits.maxVertexInputBindings ||
      description.Attributes.size() > limits.maxVertexInputAttributes) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "vertex layout exceeds device limits"});
  }
  for (std::size_t at = 0; at < description.Streams.size(); ++at) {
    const auto &stream = description.Streams[at];
    if (stream.Slot >= limits.maxVertexInputBindings || stream.Stride == 0 ||
        stream.Stride > limits.maxVertexInputBindingStride ||
        (stream.Rate != VertexRate::Vertex && stream.Rate != VertexRate::Instance) ||
        std::ranges::find(description.Streams.first(at), stream.Slot, &VertexStream::Slot) !=
            description.Streams.first(at).end()) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "invalid or duplicate vertex stream"});
    }
  }
  for (std::size_t at = 0; at < description.Attributes.size(); ++at) {
    const auto &attribute = description.Attributes[at];
    const auto stream =
        std::ranges::find(description.Streams, attribute.Stream, &VertexStream::Slot);
    const auto bytes = AttributeBytes(attribute.Format);
    if (stream == description.Streams.end() || bytes == 0 ||
        attribute.Location >= limits.maxVertexInputAttributes ||
        attribute.Offset > limits.maxVertexInputAttributeOffset ||
        attribute.Offset > stream->Stride || bytes > stream->Stride - attribute.Offset ||
        std::ranges::find(description.Attributes.first(at),
                          attribute.Location,
                          &VertexAttribute::Location) != description.Attributes.first(at).end()) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "invalid or duplicate vertex attribute"});
    }
  }
  return {};
}

bool ValidFactor(BlendFactor factor) {
  return factor == BlendFactor::Zero || factor == BlendFactor::One ||
         factor == BlendFactor::SourceAlpha || factor == BlendFactor::OneMinusSourceAlpha;
}

}

Result<void> CheckGraphics(const Context &context, const GraphicsDescription &description) {
  if (description.Colours.size() > std::min(8u, context.Limits.MaxColourAttachments) ||
      (description.Colours.empty() && !description.Depth) ||
      (!description.Colours.empty() && !description.Fragment) ||
      (description.Culling != Cull::None && description.Culling != Cull::Front &&
       description.Culling != Cull::Back) ||
      (description.Front != Winding::Clockwise && description.Front != Winding::CounterClockwise)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid graphics targets or raster state"});
  }
  for (const auto &target : description.Colours) {
    if (!Supports(context, target.Texels, TextureUse::ColourAttachment, false) ||
        (static_cast<uint32_t>(target.Write) & ~15u) != 0 || !ValidFactor(target.Rgb.Source) ||
        !ValidFactor(target.Rgb.Destination) || !ValidFactor(target.Alpha.Source) ||
        !ValidFactor(target.Alpha.Destination)) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "invalid colour format, mask or blend function"});
    }
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(context.Physical, TexelFormat(target.Texels), &properties);
    if (target.Blend &&
        (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) == 0) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "colour target cannot blend on this device"});
    }
  }
  if (description.Depth && (description.Depth->Texels != Format::Depth32Float ||
                            (description.Depth->Compare != DepthCompare::Greater &&
                             description.Depth->Compare != DepthCompare::GreaterEqual))) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid reverse-Z depth target"});
  }
  if (!context.IndependentBlend && description.Colours.size() > 1) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "independent MRT blending unavailable"});
  }
  return CheckStreams(context, description);
}

}
#endif

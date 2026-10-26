#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_VULKANGRAPHICS_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_VULKANGRAPHICS_H

#ifdef __linux__
#include "Pipeline.h"
#include <optional>

namespace outshine::Render::Native::Vulkan {

struct GraphicsReflection {
  GraphicsLayout Layout;
  std::vector<ImageRequirement> Images;
};

struct GraphicsState {
  std::shared_ptr<PipelineState> Pipeline;
  std::vector<ShaderBinding> Bindings;
  std::vector<VertexStream> Streams;
  std::vector<VertexAttribute> Attributes;
  std::vector<ColourTarget> Colours;
  std::optional<DepthTarget> Depth;
};

class VulkanGraphicsPipeline final : public GraphicsPipeline {
public:
  VulkanGraphicsPipeline(std::shared_ptr<GraphicsState> state, GraphicsLayout layout)
      : GraphicsPipeline(std::move(layout)), State_(std::move(state)) {}

  [[nodiscard]] const std::shared_ptr<GraphicsState> &State() const noexcept { return State_; }

private:
  std::shared_ptr<GraphicsState> State_;
};

[[nodiscard]] Result<GraphicsReflection> ReflectGraphics(const Context &context,
                                                         const GraphicsDescription &description);
[[nodiscard]] Result<void> CheckGraphics(const Context &context,
                                         const GraphicsDescription &description);
[[nodiscard]] Result<void> BuildGraphics(GraphicsState &state,
                                         const GraphicsDescription &description);
[[nodiscard]] Result<std::unique_ptr<GraphicsPipeline>>
CreateGraphics(std::shared_ptr<Context> context, const GraphicsDescription &description);
[[nodiscard]] VkFormat AttributeFormat(VertexFormat format) noexcept;
[[nodiscard]] uint32_t AttributeBytes(VertexFormat format) noexcept;

}
#endif
#endif

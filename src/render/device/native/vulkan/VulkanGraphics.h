#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_VULKANGRAPHICS_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_VULKANGRAPHICS_H

#ifdef __linux__
#include "Pipeline.h"
#include "GraphicsRecipe.h"
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

class GraphicsPrograms {
public:
  explicit GraphicsPrograms(std::shared_ptr<Context> context) : Context_(std::move(context)) {}

  GraphicsPrograms(const GraphicsPrograms &) = delete;
  GraphicsPrograms &operator=(const GraphicsPrograms &) = delete;

  [[nodiscard]] Result<std::unique_ptr<GraphicsPipeline>>
  Create(const GraphicsDescription &description);

  [[nodiscard]] size_t StoredPrograms() const noexcept { return Entries_.size(); }

  [[nodiscard]] size_t RecipeCapacityBytes() const noexcept;

private:
  struct Entry {
    std::weak_ptr<PipelineState> Pipeline;
    GraphicsLayout Layout;
  };

  struct Found {
    std::shared_ptr<PipelineState> Pipeline;
    GraphicsLayout Layout;
  };

  [[nodiscard]] std::optional<Found> Find(const GraphicsRecipe &recipe);
  void Remember(GraphicsRecipe recipe,
                const std::shared_ptr<PipelineState> &pipeline,
                const GraphicsLayout &layout);

  std::shared_ptr<Context> Context_;
  std::map<GraphicsRecipe, Entry> Entries_;
};

[[nodiscard]] Result<GraphicsReflection> ReflectGraphics(const Context &context,
                                                         const GraphicsDescription &description);
[[nodiscard]] Result<void> CheckGraphics(const Context &context,
                                         const GraphicsDescription &description);
[[nodiscard]] Result<void> BuildGraphics(GraphicsState &state,
                                         const GraphicsDescription &description);
[[nodiscard]] VkFormat AttributeFormat(VertexFormat format) noexcept;
[[nodiscard]] uint32_t AttributeBytes(VertexFormat format) noexcept;

}
#endif
#endif

#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALGRAPHICS_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALGRAPHICS_H

#include "../Graphics.h"
#include "MetalShader.h"
#import <Metal/Metal.h>
#include <memory>

namespace outshine::Render::Native::Metal {

inline constexpr uint32_t kFirstVertexBuffer = kBufferSizesSlot + 1;
inline constexpr uint32_t kMetalBufferSlots = 31;
inline constexpr uint32_t kVertexAttributeSlots = 31;
inline constexpr uint32_t kVertexBufferSlots = kMetalBufferSlots - kFirstVertexBuffer;

struct GraphicsState {
  id<MTLRenderPipelineState> Native;
  id<MTLDepthStencilState> Depth;
  Shader Vertex;
  std::optional<Shader> Fragment;
  std::vector<VertexStream> Streams;
  std::vector<VertexAttribute> Attributes;
  std::vector<ColourTarget> Colours;
  std::optional<DepthTarget> DepthSettings;
  MTLCullMode Culling = MTLCullModeNone;
  MTLWinding Front = MTLWindingCounterClockwise;
  uint64_t Device = 0;
};

class MetalGraphicsPipeline final : public GraphicsPipeline {
public:
  MetalGraphicsPipeline(GraphicsLayout layout, std::shared_ptr<GraphicsState> state)
      : GraphicsPipeline(std::move(layout)), State(std::move(state)) {}

  std::shared_ptr<GraphicsState> State;
};

}
#endif

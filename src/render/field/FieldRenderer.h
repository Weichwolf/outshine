#ifndef OUTSHINE_RENDER_FIELD_FIELDRENDERER_H
#define OUTSHINE_RENDER_FIELD_FIELDRENDERER_H

#include "FieldSurface.h"
#include "GpuPlacement.h"
#include "native/Device.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Render {
class UploadQueue;

struct FieldDraw {
  uint64_t Id = 0;
  uint32_t MaterialSlot = 0;
  uint32_t IndexCount = 0;
  FieldMaskGpu Parameters;
  std::unique_ptr<Native::Buffer> Vertices, Indices, Placement;
  std::unique_ptr<Native::Texture> Heights;
  std::unique_ptr<Native::Sampler> Sampling;

  [[nodiscard]] std::array<Native::Binding, 3> Bindings() const;
  [[nodiscard]] Native::Result<void> Encode(Native::Commands &commands,
                                            const Native::GraphicsPipeline &pipeline,
                                            std::span<const Native::Binding> bindings,
                                            Native::Viewport area,
                                            Native::Scissor clip) const;
};

class FieldRenderer {
public:
  struct AllocationStats {
    uint64_t VertexBytes = 0, IndexBytes = 0, PlacementBytes = 0, HeightPayloadBytes = 0;
  };

  [[nodiscard]] AllocationStats Allocations() const;
  [[nodiscard]] Native::Result<uint64_t> Place(Native::Device &device,
                                               UploadQueue *uploads,
                                               const FieldMaskGpu &parameters,
                                               std::span<const float> heights,
                                               uint32_t materialSlot,
                                               const GpuPlacement &placement);
  [[nodiscard]] bool Release(uint64_t id);

  [[nodiscard]] std::span<const FieldDraw> Drawn() const { return Draws_; }

  [[nodiscard]] uint64_t Generation() const { return Generation_; }

private:
  std::vector<FieldDraw> Draws_;
  uint64_t Generation_ = 0;
};
}
#endif

#ifndef OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDRASTER_H
#define OUTSHINE_RENDER_IMPOSTOR_MATERIALFIELDRASTER_H
#include "native/Device.h"
#include "Extent.h"
#include <memory>

namespace outshine::Render {
struct MaterialFieldRasterInput;
struct MaterialFieldView;
struct FrameContext;

struct MaterialFieldRaster {
  std::unique_ptr<Native::Buffer> Pixels;
  std::unique_ptr<Native::Buffer> Staging;
  std::unique_ptr<Native::Texture> Empty;
  [[nodiscard]] static Native::Result<std::unique_ptr<MaterialFieldRaster>> Prepare(
      Native::Device &device, Native::Commands &commands, const MaterialFieldRasterInput &input);
  [[nodiscard]] Native::Result<void> Encode(Native::Commands &commands,
                                            const Native::GraphicsPipeline &pipeline,
                                            const Native::Sampler &sampling,
                                            const MaterialFieldView &view,
                                            const FrameContext &frame,
                                            Extent extent) const;
};
}
#endif

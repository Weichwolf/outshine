#ifndef OUTSHINE_RENDER_FIELD_FIELDPIPELINES_H
#define OUTSHINE_RENDER_FIELD_FIELDPIPELINES_H
#include "native/Device.h"
#include "SurfaceOutputs.h"
#include <memory>
#include <span>

namespace outshine::Render {
[[nodiscard]] Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
ConfigureFieldColour(Native::Device &device,
                     const SurfaceOutputs &outputs,
                     std::span<const Native::ColourTarget> targets);
[[nodiscard]] Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
ConfigureFieldDepth(Native::Device &device, Native::Cull culling = Native::Cull::Back);
}
#endif

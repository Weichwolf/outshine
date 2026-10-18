#ifndef OUTSHINE_RENDER_STAGES_STAGEPIPELINES_H
#define OUTSHINE_RENDER_STAGES_STAGEPIPELINES_H

#include "ComputeShaders.h"
#include "Device.h"

#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace outshine::Render {

[[nodiscard]] Native::Result<std::unique_ptr<Native::ComputePipeline>>
CreateComputePipeline(Native::Device &device, ComputeShaderId shader);

[[nodiscard]] Native::Result<std::unique_ptr<Native::ComputePipeline>>
CompileComputePipeline(Native::Device &device, const ComputeShaderDescriptor &shader);

[[nodiscard]] Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
CreatePresentPipeline(Native::Device &device, Native::Format target);

[[nodiscard]] Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
CreateFullscreenPipeline(Native::Device &device,
                         std::string_view fragmentPath,
                         std::span<const Native::ColourTarget> colours,
                         std::optional<Native::DepthTarget> depth = {});

[[nodiscard]] Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
CreateFullscreenNdcPipeline(Native::Device &device,
                            std::string_view fragmentPath,
                            std::span<const Native::ColourTarget> colours,
                            std::optional<Native::DepthTarget> depth = {});

}
#endif

#ifndef OUTSHINE_RENDER_STAGES_STAGEPIPELINES_H
#define OUTSHINE_RENDER_STAGES_STAGEPIPELINES_H

#include "ComputeShaders.h"
#include "Device.h"

#include <memory>

namespace outshine::Render {

[[nodiscard]] Native::Result<std::unique_ptr<Native::ComputePipeline>>
CreateComputePipeline(Native::Device &device, ComputeShaderId shader);

[[nodiscard]] Native::Result<std::unique_ptr<Native::ComputePipeline>>
CompileComputePipeline(Native::Device &device, const ComputeShaderDescriptor &shader);

}
#endif

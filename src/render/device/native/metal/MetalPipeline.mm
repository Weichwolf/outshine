#include "MetalInternal.h"
#include <algorithm>

namespace outshine::Render::Native::Metal {

Result<std::unique_ptr<ComputePipeline>>
MetalDevice::CreateComputePipeline(std::span<const uint32_t> spirv, ComputeOptions settings) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    const auto shader = ShaderProgram(spirv, settings.Entry);
    if (!shader) { return std::unexpected(shader.error()); }
    uint64_t invocations = 1;
    for (size_t axis = 0; axis < (*shader)->Translated.Layout.LocalSize.size(); ++axis) {
      const auto size = (*shader)->Translated.Layout.LocalSize[axis];
      if (size == 0 || size > Limits_.MaxWorkgroupSize[axis]) {
        return Failure(ErrorKind::Invalid, "Metal compute local size exceeds device limits");
      }
      invocations *= size;
    }
    const auto function = Function(**shader, MTLFunctionTypeKernel);
    if (!function) { return std::unexpected(function.error()); }
    NSError *error = nil;
    MTLComputePipelineDescriptor *const description = [MTLComputePipelineDescriptor new];
    description.computeFunction = *function;
    description.label = Text(settings.Label.empty() ? settings.Entry : settings.Label);
    const id<MTLComputePipelineState> pipeline =
        [Native_ newComputePipelineStateWithDescriptor:description
                                               options:MTLPipelineOptionNone
                                            reflection:nil
                                                 error:&error];
    if (pipeline == nil) {
      return Failure(ErrorKind::Driver, error.localizedDescription.UTF8String);
    }
    if (invocations > pipeline.maxTotalThreadsPerThreadgroup) {
      return Failure(ErrorKind::Invalid, "Metal compute workgroup exceeds pipeline limits");
    }
    auto state = std::make_shared<PipelineState>();
    state->Native = pipeline;
    state->Translated = (*shader)->Translated;
    state->Device = Identity_;
    Limits_.MaxWorkgroupInvocations =
        std::max(Limits_.MaxWorkgroupInvocations,
                 static_cast<uint32_t>(pipeline.maxTotalThreadsPerThreadgroup));
    return std::make_unique<MetalPipeline>(std::move(state));
  }
}

}

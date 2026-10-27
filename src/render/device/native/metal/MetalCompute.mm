#include "MetalInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace outshine::Render::Native::Metal {

static_assert(sizeof(MTLDispatchThreadgroupsIndirectArguments) == sizeof(Workgroups));
static_assert(alignof(MTLDispatchThreadgroupsIndirectArguments) == alignof(Workgroups));
static_assert(offsetof(MTLDispatchThreadgroupsIndirectArguments, threadgroupsPerGrid) ==
              offsetof(Workgroups, X));

Result<void> MetalCommands::PrepareCompute(const MetalPipeline &pipeline,
                                           std::span<const Binding> bindings,
                                           const MetalBuffer *indirect,
                                           uint64_t offset) {
  const auto arguments = Arguments(*pipeline.State, bindings, Device, ArgumentBuffers[0]);
  if (!arguments) { return std::unexpected(arguments.error()); }
  if (indirect != nullptr) {
    for (const auto &argument : *arguments) {
      if (argument.Buffer && argument.Buffer->Native == indirect->State->Native &&
          argument.Storage != nullptr && argument.Layout->Source.Usage != Access::Read) {
        const auto range = argument.Storage->Range;
        if (range.Offset < offset + sizeof(Workgroups) && offset < range.Offset + range.Bytes) {
          return Failure(ErrorKind::Invalid, "Metal dispatch arguments overlap writable storage");
        }
      }
    }
  }
  std::vector<id<MTLResource>> hazards;
  const auto prepared = PrepareResources(*arguments, hazards);
  if (!prepared) { return prepared; }
  if (indirect != nullptr) {
    const std::array reads{indirect->State};
    const auto held = PrepareReads(reads, hazards);
    if (!held) { return held; }
  }
  if (Compute == nil && Profile) {
    MTLComputePassDescriptor *const description = [MTLComputePassDescriptor computePassDescriptor];
    ProfileComputeEncoder(*Profile, description, pipeline.State->Native.label);
    Compute = [Native computeCommandEncoderWithDescriptor:description];
  } else if (Compute == nil) {
    Compute = [Native computeCommandEncoder];
  }
  if (Compute == nil) {
    return Failure(ErrorKind::Driver, "Metal compute encoder creation failed");
  }
  if (!hazards.empty()) {
    [Compute memoryBarrierWithResources:hazards.data() count:hazards.size()];
    std::erase_if(Accesses, [&](const TrackedResource &buffer) {
      return std::ranges::find(hazards, buffer.Resource) != hazards.end();
    });
  }
  [Compute setComputePipelineState:pipeline.State->Native];
  const auto bound = Bind(Compute, pipeline.State->Translated, *arguments);
  if (!bound) { return bound; }
  UpdateAccesses(*arguments);
  if (indirect != nullptr) { UpdateAccess(indirect->State->Native, Access::Read); }
  return {};
}

Result<void> MetalCommands::Dispatch(const ComputePipeline &pipeline,
                                     std::span<const Binding> bindings,
                                     Workgroups groups) {
  if (!OnOwner() || Render) {
    return Failure(ErrorKind::Invalid, "Metal dispatch requires its owner outside a render pass");
  }
  @autoreleasepool {
    const auto *const metal = dynamic_cast<const MetalPipeline *>(&pipeline);
    if (metal == nullptr || metal->State->Device != Device || groups.X == 0 || groups.Y == 0 ||
        groups.Z == 0) {
      return Failure(ErrorKind::Invalid, "invalid Metal compute pipeline, ownership or workgroups");
    }
    const std::array counts{groups.X, groups.Y, groups.Z};
    for (size_t axis = 0; axis < counts.size(); ++axis) {
      if (uint64_t{counts[axis]} * pipeline.Layout().LocalSize[axis] >
          std::numeric_limits<uint32_t>::max()) {
        return Failure(ErrorKind::Invalid, "Metal dispatch exceeds 32-bit shader coordinates");
      }
    }
    const ArgumentScope argumentScope(ArgumentBuffers);
    const auto prepared = PrepareCompute(*metal, bindings);
    if (!prepared) { return prepared; }
    const auto size = pipeline.Layout().LocalSize;
    [Compute dispatchThreadgroups:MTLSizeMake(groups.X, groups.Y, groups.Z)
            threadsPerThreadgroup:MTLSizeMake(size[0], size[1], size[2])];
    return {};
  }
}

Result<void> MetalCommands::DispatchIndirect(const ComputePipeline &pipeline,
                                             std::span<const Binding> bindings,
                                             const Buffer &arguments,
                                             uint64_t offset) {
  if (!OnOwner() || Render) {
    return Failure(ErrorKind::Invalid, "Metal dispatch requires its owner outside a render pass");
  }
  @autoreleasepool {
    const auto *const metal = dynamic_cast<const MetalPipeline *>(&pipeline);
    const auto *const indirect = dynamic_cast<const MetalBuffer *>(&arguments);
    if (metal == nullptr || metal->State->Device != Device || indirect == nullptr ||
        indirect->State->Device != Device ||
        !Has(arguments.Description().Use, BufferUse::Indirect) ||
        offset % alignof(Workgroups) != 0 || offset > arguments.Description().Bytes ||
        sizeof(Workgroups) > arguments.Description().Bytes - offset) {
      return Failure(ErrorKind::Invalid,
                     "invalid Metal indirect dispatch buffer, range or ownership");
    }
    const ArgumentScope argumentScope(ArgumentBuffers);
    const auto prepared = PrepareCompute(*metal, bindings, indirect, offset);
    if (!prepared) { return prepared; }
    const auto size = pipeline.Layout().LocalSize;
    [Compute dispatchThreadgroupsWithIndirectBuffer:indirect->State->Native
                               indirectBufferOffset:offset
                              threadsPerThreadgroup:MTLSizeMake(size[0], size[1], size[2])];
    return {};
  }
}

}

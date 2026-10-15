#include "MetalInternal.h"
#include <algorithm>
#include <limits>

namespace outshine::Render::Native::Metal {

Reservation::~Reservation() {
  if (State) {
    const std::scoped_lock lock(State->Mutex);
    --State->Uses;
  }
}

MetalCommands::~MetalCommands() {
  Finish();
  ReleaseImages();
}

Result<void> MetalCommands::HoldImages(std::span<const std::shared_ptr<TextureRecording>> images) {
  for (const auto &image : images) {
    if (image->Owner != nullptr && image->Owner != this) {
      return Failure(ErrorKind::Busy, "Metal texture belongs to another unsubmitted recording");
    }
  }
  for (const auto &image : images) {
    if (image->Owner == this) { continue; }
    image->Owner = this;
    HeldImages.push_back(image);
  }
  return {};
}

void MetalCommands::ReleaseImages() {
  for (const auto &image : HeldImages) { image->Owner = nullptr; }
  HeldImages.clear();
}

void MetalCommands::Finish() {
  if (Render) {
    [Render->Encoder endEncoding];
    Render.reset();
  }
  if (Compute != nil) {
    [Compute endEncoding];
    Compute = nil;
  }
  Accesses.clear();
}

Result<void> MetalCommands::Hold(const std::shared_ptr<BufferState> &state) {
  if (std::ranges::any_of(Held, [&](const Reservation &one) { return one.State == state; })) {
    return {};
  }
  const std::scoped_lock lock(state->Mutex);
  if (state->Mapped) { return Failure(ErrorKind::Busy, "Metal buffer is still mapped"); }
  ++state->Uses;
  Held.emplace_back(state);
  return {};
}

Result<void> MetalCommands::Copy(const Buffer &source, Buffer &destination, BufferCopy region) {
  if (!OnOwner() || Render) {
    return Failure(ErrorKind::Invalid, "Metal copy requires its owner outside a render pass");
  }
  @autoreleasepool {
    const auto *const from = dynamic_cast<const MetalBuffer *>(&source);
    const auto *const to = dynamic_cast<const MetalBuffer *>(&destination);
    if (from == nullptr || to == nullptr || from->State->Device != Device ||
        to->State->Device != Device || !Has(source.Description().Use, BufferUse::CopySource) ||
        !Has(destination.Description().Use, BufferUse::CopyDestination) ||
        !Fits({.Offset = region.SourceOffset, .Bytes = region.Bytes}, source.Description().Bytes) ||
        !Fits({.Offset = region.DestinationOffset, .Bytes = region.Bytes},
              destination.Description().Bytes) ||
        region.SourceOffset % 4 != 0 || region.DestinationOffset % 4 != 0 ||
        region.Bytes % 4 != 0 || from->State == to->State) {
      return Failure(ErrorKind::Invalid, "invalid Metal buffer copy or ownership");
    }
    auto held = Hold(from->State);
    if (!held) { return held; }
    held = Hold(to->State);
    if (!held) { return held; }
    Finish();
    const id<MTLBlitCommandEncoder> blit = [Native blitCommandEncoder];
    if (blit == nil) { return Failure(ErrorKind::Driver, "Metal blit encoder creation failed"); }
    [blit copyFromBuffer:from->State->Native
             sourceOffset:region.SourceOffset
                 toBuffer:to->State->Native
        destinationOffset:region.DestinationOffset
                     size:region.Bytes];
    [blit endEncoding];
    return {};
  }
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
    const auto arguments = Arguments(*metal->State, bindings, Device);
    if (!arguments) { return std::unexpected(arguments.error()); }
    std::vector<id<MTLResource>> hazards;
    const auto prepared = PrepareResources(*arguments, hazards);
    if (!prepared) { return prepared; }
    if (Compute == nil) { Compute = [Native computeCommandEncoder]; }
    if (Compute == nil) {
      return Failure(ErrorKind::Driver, "Metal compute encoder creation failed");
    }
    if (!hazards.empty()) {
      [Compute memoryBarrierWithResources:hazards.data() count:hazards.size()];
      std::erase_if(Accesses, [&](const TrackedResource &buffer) {
        return std::ranges::find(hazards, buffer.Resource) != hazards.end();
      });
    }
    [Compute setComputePipelineState:metal->State->Native];
    const auto bound = Bind(Compute, metal->State->Translated, *arguments);
    if (!bound) { return bound; }
    const auto size = pipeline.Layout().LocalSize;
    [Compute dispatchThreadgroups:MTLSizeMake(groups.X, groups.Y, groups.Z)
            threadsPerThreadgroup:MTLSizeMake(size[0], size[1], size[2])];
    UpdateAccesses(*arguments);
    return {};
  }
}

void MetalCommands::TrackHazard(id<MTLResource> resource,
                                Access usage,
                                std::vector<id<MTLResource>> &hazards) const {
  const auto previous = std::ranges::find(Accesses, resource, &TrackedResource::Resource);
  if (previous != Accesses.end() && (previous->Usage != Access::Read || usage != Access::Read) &&
      std::ranges::find(hazards, resource) == hazards.end()) {
    hazards.push_back(resource);
  }
}

Result<void> MetalCommands::PrepareReads(std::span<const std::shared_ptr<BufferState>> buffers,
                                         std::vector<id<MTLResource>> &hazards) {
  for (const auto &buffer : buffers) {
    const auto held = Hold(buffer);
    if (!held) { return held; }
    TrackHazard(buffer->Native, Access::Read, hazards);
  }
  return {};
}

Result<void> MetalCommands::PrepareResources(std::span<const ShaderArgument> arguments,
                                             std::vector<id<MTLResource>> &hazards) {
  std::vector<std::shared_ptr<TextureRecording>> images;
  for (const auto &argument : arguments) {
    if (argument.Buffer) {
      const auto held = Hold(argument.Buffer);
      if (!held) { return held; }
    }
    if (argument.Resource != nil) {
      TrackHazard(argument.Resource, argument.Layout->Source.Usage, hazards);
    }
    if (argument.Recording) { images.push_back(argument.Recording); }
  }
  return HoldImages(images);
}

void MetalCommands::UpdateAccess(id<MTLResource> resource, Access usage) {
  if (resource == nil) { return; }
  const auto existing = std::ranges::find(Accesses, resource, &TrackedResource::Resource);
  if (existing == Accesses.end()) {
    Accesses.push_back({.Resource = resource, .Usage = usage});
  } else if (existing->Usage != usage) {
    existing->Usage = Access::ReadWrite;
  }
}

void MetalCommands::UpdateAccesses(std::span<const ShaderArgument> arguments) {
  for (const auto &argument : arguments) {
    UpdateAccess(argument.Resource, argument.Layout->Source.Usage);
  }
}

void MetalCommands::BeginLabel(std::string_view name) {
  @autoreleasepool {
    [Native pushDebugGroup:Text(name)];
  }
}

void MetalCommands::EndLabel() {
  @autoreleasepool {
    [Native popDebugGroup];
  }
}

}

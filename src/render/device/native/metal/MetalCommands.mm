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
}

void MetalCommands::Finish() {
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

Result<void> MetalCommands::Copy([[maybe_unused]] const Buffer &source,
                                 [[maybe_unused]] Texture &destination,
                                 [[maybe_unused]] TextureTransfer region) {
  return Failure(ErrorKind::Unsupported, "Metal texture uploads are not integrated yet");
}

Result<void> MetalCommands::Copy([[maybe_unused]] const Texture &source,
                                 [[maybe_unused]] Buffer &destination,
                                 [[maybe_unused]] TextureTransfer region) {
  return Failure(ErrorKind::Unsupported, "Metal texture readbacks are not integrated yet");
}

Result<void> MetalCommands::Copy([[maybe_unused]] const Texture &source,
                                 [[maybe_unused]] TextureRegion sourceRegion,
                                 [[maybe_unused]] Texture &destination,
                                 [[maybe_unused]] TextureRegion destinationRegion) {
  return Failure(ErrorKind::Unsupported, "Metal texture copies are not integrated yet");
}

namespace {

struct Argument {
  const ResourceBinding *Layout = nullptr;
  const StorageBuffer *Storage = nullptr;
  const Uniform *Constants = nullptr;
  std::shared_ptr<BufferState> Buffer;
};

Result<std::vector<Argument>>
Arguments(const PipelineState &pipeline, std::span<const Binding> bindings, uint64_t device) {
  if (bindings.size() != pipeline.Translated.Resources.size()) {
    return Failure(ErrorKind::Invalid, "Metal dispatch bindings differ from reflection");
  }
  std::vector<Argument> result;
  result.reserve(bindings.size());
  for (const auto &layout : pipeline.Translated.Resources) {
    const auto matches = [&](const Binding &value) {
      return value.Set == layout.Source.Set && value.Slot == layout.Source.Slot;
    };
    if (std::ranges::count_if(bindings, matches) != 1) {
      return Failure(ErrorKind::Invalid, "missing or repeated Metal dispatch binding");
    }
    const auto &binding = *std::ranges::find_if(bindings, matches);
    Argument argument{.Layout = &layout};
    if (layout.Source.Kind == BindingKind::Uniform) {
      argument.Constants = std::get_if<Uniform>(&binding.Value);
      constexpr size_t maxUniformBytes = 64L * 1024;
      if (argument.Constants == nullptr ||
          argument.Constants->Bytes.size() < layout.Source.MinimumBytes ||
          argument.Constants->Bytes.empty() || argument.Constants->Bytes.size() > maxUniformBytes) {
        return Failure(ErrorKind::Invalid, "invalid Metal uniform byte span");
      }
    } else {
      argument.Storage = std::get_if<StorageBuffer>(&binding.Value);
      const auto *const buffer = argument.Storage != nullptr
                                     ? dynamic_cast<const MetalBuffer *>(argument.Storage->Data)
                                     : nullptr;
      if (buffer == nullptr || buffer->State->Device != device ||
          !Has(buffer->Description().Use, BufferUse::Storage) ||
          !Fits(argument.Storage->Range, buffer->Description().Bytes) ||
          argument.Storage->Range.Offset % 4 != 0 ||
          argument.Storage->Range.Bytes < layout.Source.MinimumBytes ||
          argument.Storage->Range.Bytes > std::numeric_limits<uint32_t>::max()) {
        return Failure(ErrorKind::Invalid, "invalid Metal storage buffer binding or ownership");
      }
      argument.Buffer = buffer->State;
    }
    result.push_back(std::move(argument));
  }
  return result;
}

Result<void> Bind(id<MTLComputeCommandEncoder> encoder,
                  const Shader &shader,
                  const std::vector<Argument> &arguments) {
  std::array<uint32_t, kBufferSizesSlot> sizes{};
  for (const auto &argument : arguments) {
    const auto slot = argument.Layout->Buffer;
    if (argument.Storage != nullptr) {
      [encoder setBuffer:argument.Buffer->Native
                  offset:argument.Storage->Range.Offset
                 atIndex:slot];
      sizes[slot] = static_cast<uint32_t>(argument.Storage->Range.Bytes);
    } else {
      const auto bytes = argument.Constants->Bytes;
      constexpr size_t inlineBytes = 4096;
      if (bytes.size() <= inlineBytes) {
        [encoder setBytes:bytes.data() length:bytes.size() atIndex:slot];
      } else {
        const id<MTLBuffer> constants =
            [encoder.device newBufferWithBytes:bytes.data()
                                        length:bytes.size()
                                       options:MTLResourceStorageModeShared];
        if (constants == nil) {
          return Failure(ErrorKind::OutOfMemory, "Metal uniform allocation failed");
        }
        [encoder setBuffer:constants offset:0 atIndex:slot];
      }
    }
  }
  if (shader.NeedsBufferSizes) {
    [encoder setBytes:sizes.data() length:sizeof(sizes) atIndex:kBufferSizesSlot];
  }
  return {};
}

}

Result<void> MetalCommands::Dispatch(const ComputePipeline &pipeline,
                                     std::span<const Binding> bindings,
                                     Workgroups groups) {
  @autoreleasepool {
    const auto *const metal = dynamic_cast<const MetalPipeline *>(&pipeline);
    if (metal == nullptr || metal->State->Device != Device || groups.X == 0 || groups.Y == 0 ||
        groups.Z == 0) {
      return Failure(ErrorKind::Invalid, "invalid Metal compute pipeline, ownership or workgroups");
    }
    const auto arguments = Arguments(*metal->State, bindings, Device);
    if (!arguments) { return std::unexpected(arguments.error()); }
    std::vector<id<MTLResource>> hazards;
    for (const auto &argument : *arguments) {
      if (argument.Buffer) {
        const auto held = Hold(argument.Buffer);
        if (!held) { return held; }
        const auto previous =
            std::ranges::find(Accesses, argument.Buffer->Native, &TrackedBuffer::Buffer);
        if (previous != Accesses.end() &&
            (previous->Usage != Access::Read || argument.Layout->Source.Usage != Access::Read)) {
          hazards.push_back(argument.Buffer->Native);
        }
      }
    }
    if (Compute == nil) { Compute = [Native computeCommandEncoder]; }
    if (Compute == nil) {
      return Failure(ErrorKind::Driver, "Metal compute encoder creation failed");
    }
    if (!hazards.empty()) {
      [Compute memoryBarrierWithResources:hazards.data() count:hazards.size()];
      std::erase_if(Accesses, [&](const TrackedBuffer &buffer) {
        return std::ranges::find(hazards, buffer.Buffer) != hazards.end();
      });
    }
    [Compute setComputePipelineState:metal->State->Native];
    const auto bound = Bind(Compute, metal->State->Translated, *arguments);
    if (!bound) { return bound; }
    const auto size = pipeline.Layout().LocalSize;
    [Compute dispatchThreadgroups:MTLSizeMake(groups.X, groups.Y, groups.Z)
            threadsPerThreadgroup:MTLSizeMake(size[0], size[1], size[2])];
    for (const auto &argument : *arguments) {
      if (!argument.Buffer) { continue; }
      const auto existing =
          std::ranges::find(Accesses, argument.Buffer->Native, &TrackedBuffer::Buffer);
      if (existing == Accesses.end()) {
        Accesses.push_back(
            {.Buffer = argument.Buffer->Native, .Usage = argument.Layout->Source.Usage});
      } else if (existing->Usage != argument.Layout->Source.Usage) {
        existing->Usage = Access::ReadWrite;
      }
    }
    return {};
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

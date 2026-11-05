#include "MetalInternal.h"
#include <algorithm>
#include <limits>

namespace outshine::Render::Native::Metal {
Result<ArgumentSlice>
ArgumentArena::Allocate(id<MTLDevice> device, uint64_t bytes, uint64_t alignment) {
  if (bytes == 0 || alignment == 0 || Cursor > std::numeric_limits<uint64_t>::max() - alignment) {
    return Failure(ErrorKind::Invalid, "invalid Metal argument allocation");
  }
  const uint64_t offset = Cursor + (alignment - Cursor % alignment) % alignment;
  if (Blocks.empty() || offset > Blocks.back().length || bytes > Blocks.back().length - offset) {
    constexpr uint64_t allocationQuantum = uint64_t{64} * 1024;
    const uint64_t length = std::max(bytes, allocationQuantum);
    const id<MTLBuffer> block = [device newBufferWithLength:length
                                                    options:MTLResourceStorageModeShared];
    if (block == nil) {
      return Failure(ErrorKind::OutOfMemory, "Metal argument arena allocation failed");
    }
    Blocks.push_back(block);
    Cursor = bytes;
    return ArgumentSlice{.Buffer = block};
  }
  Cursor = offset + bytes;
  return ArgumentSlice{.Buffer = Blocks.back(), .Offset = offset};
}

Result<ArgumentSlice> EncodeTextureArguments(const RenderState &pass,
                                             ShaderStage stage,
                                             id<MTLArgumentEncoder> encoder,
                                             std::span<const ShaderArgument> arguments,
                                             ArgumentArena &arena) {
  const auto slice = arena.Allocate(pass.Encoder.device, encoder.encodedLength, encoder.alignment);
  if (!slice) { return slice; }
  [encoder setArgumentBuffer:slice->Buffer offset:slice->Offset];
  const MTLRenderStages stages =
      stage == ShaderStage::Vertex ? MTLRenderStageVertex : MTLRenderStageFragment;
  for (const auto &argument : arguments) {
    if (argument.Texture == nil) { continue; }
    [encoder setTexture:argument.Texture atIndex:argument.Layout->Texture];
    const auto access = argument.Layout->Source.Usage;
    MTLResourceUsage usage = access == Access::Read ? MTLResourceUsageRead : MTLResourceUsageWrite;
    if (access == Access::ReadWrite) { usage = MTLResourceUsageRead | MTLResourceUsageWrite; }
    [pass.Encoder useResource:argument.Texture usage:usage stages:stages];
    if (argument.Sampler != nil) {
      [encoder setSamplerState:argument.Sampler atIndex:argument.Layout->Sampler];
      if (std::ranges::find(arena.Samplers, argument.Sampler) == arena.Samplers.end()) {
        arena.Samplers.push_back(argument.Sampler);
      }
    }
  }
  [encoder setArgumentBuffer:nil offset:0];
  return slice;
}
}

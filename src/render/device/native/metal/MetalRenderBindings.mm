#include "MetalInternal.h"
#include <array>

namespace outshine::Render::Native::Metal {
namespace {

void BindBuffer(id<MTLRenderCommandEncoder> encoder,
                ShaderStage stage,
                id<MTLBuffer> buffer,
                uint64_t offset,
                uint32_t slot) {
  if (stage == ShaderStage::Vertex) {
    [encoder setVertexBuffer:buffer offset:offset atIndex:slot];
  } else {
    [encoder setFragmentBuffer:buffer offset:offset atIndex:slot];
  }
}

void BindBytes(id<MTLRenderCommandEncoder> encoder,
               ShaderStage stage,
               std::span<const std::byte> bytes,
               uint32_t slot) {
  if (stage == ShaderStage::Vertex) {
    [encoder setVertexBytes:bytes.data() length:bytes.size() atIndex:slot];
  } else {
    [encoder setFragmentBytes:bytes.data() length:bytes.size() atIndex:slot];
  }
}

void BindTexture(id<MTLRenderCommandEncoder> encoder,
                 ShaderStage stage,
                 const ShaderArgument &argument) {
  if (stage == ShaderStage::Vertex) {
    [encoder setVertexTexture:argument.Texture atIndex:argument.Layout->Texture];
    if (argument.Sampler != nil) {
      [encoder setVertexSamplerState:argument.Sampler atIndex:argument.Layout->Sampler];
    }
  } else {
    [encoder setFragmentTexture:argument.Texture atIndex:argument.Layout->Texture];
    if (argument.Sampler != nil) {
      [encoder setFragmentSamplerState:argument.Sampler atIndex:argument.Layout->Sampler];
    }
  }
}

}

Result<void> Bind(id<MTLRenderCommandEncoder> encoder,
                  ShaderStage stage,
                  const Shader &shader,
                  const std::vector<ShaderArgument> &arguments) {
  std::array<uint32_t, kBufferSizesSlot> sizes{};
  for (const auto &argument : arguments) {
    const auto slot = argument.Layout->Buffer;
    if (argument.Storage != nullptr) {
      BindBuffer(encoder, stage, argument.Buffer->Native, argument.Storage->Range.Offset, slot);
      sizes[slot] = static_cast<uint32_t>(argument.Storage->Range.Bytes);
    } else if (argument.Texture != nil) {
      BindTexture(encoder, stage, argument);
    } else {
      const auto bytes = argument.Constants->Bytes;
      constexpr size_t inlineBytes = 4096;
      if (bytes.size() <= inlineBytes) {
        BindBytes(encoder, stage, bytes, slot);
      } else {
        const id<MTLBuffer> constants =
            [encoder.device newBufferWithBytes:bytes.data()
                                        length:bytes.size()
                                       options:MTLResourceStorageModeShared];
        if (constants == nil) {
          return Failure(ErrorKind::OutOfMemory, "Metal render uniform allocation failed");
        }
        BindBuffer(encoder, stage, constants, 0, slot);
      }
    }
  }
  if (shader.NeedsBufferSizes) {
    BindBytes(encoder, stage, std::as_bytes(std::span(sizes)), kBufferSizesSlot);
  }
  return {};
}

}

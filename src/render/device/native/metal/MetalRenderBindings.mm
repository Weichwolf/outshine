#include "MetalInternal.h"
#include <array>

namespace outshine::Render::Native::Metal {
namespace {

void BindBuffer(
    RenderState &pass, ShaderStage stage, id<MTLBuffer> buffer, uint64_t offset, uint32_t slot) {
  auto &bound = pass.Bindings[stage == ShaderStage::Vertex ? 0u : 1u].Buffers[slot];
  if (bound.Buffer == buffer && bound.Offset == offset) { return; }
  const auto encoder = pass.Encoder;
  if (stage == ShaderStage::Vertex) {
    [encoder setVertexBuffer:buffer offset:offset atIndex:slot];
  } else {
    [encoder setFragmentBuffer:buffer offset:offset atIndex:slot];
  }
  bound = {.Buffer = buffer, .Offset = offset};
}

void BindBytes(RenderState &pass,
               ShaderStage stage,
               std::span<const std::byte> bytes,
               uint32_t slot) {
  auto &bindings = pass.Bindings[stage == ShaderStage::Vertex ? 0u : 1u];
  if (slot < bindings.Buffers.size()) { bindings.Buffers[slot] = {}; }
  const auto encoder = pass.Encoder;
  if (stage == ShaderStage::Vertex) {
    [encoder setVertexBytes:bytes.data() length:bytes.size() atIndex:slot];
  } else {
    [encoder setFragmentBytes:bytes.data() length:bytes.size() atIndex:slot];
  }
}

void BindTexture(RenderState &pass, ShaderStage stage, const ShaderArgument &argument) {
  auto &bindings = pass.Bindings[stage == ShaderStage::Vertex ? 0u : 1u];
  const auto encoder = pass.Encoder;
  const auto texture = argument.Layout->Texture;
  if (texture >= bindings.Textures.size() || bindings.Textures[texture] != argument.Texture) {
    if (stage == ShaderStage::Vertex) {
      [encoder setVertexTexture:argument.Texture atIndex:texture];
    } else {
      [encoder setFragmentTexture:argument.Texture atIndex:texture];
    }
    if (texture < bindings.Textures.size()) { bindings.Textures[texture] = argument.Texture; }
  }
  const auto sampler = argument.Layout->Sampler;
  if (argument.Sampler != nil &&
      (sampler >= bindings.Samplers.size() || bindings.Samplers[sampler] != argument.Sampler)) {
    if (stage == ShaderStage::Vertex) {
      [encoder setVertexSamplerState:argument.Sampler atIndex:sampler];
    } else {
      [encoder setFragmentSamplerState:argument.Sampler atIndex:sampler];
    }
    if (sampler < bindings.Samplers.size()) { bindings.Samplers[sampler] = argument.Sampler; }
  }
}

}

Result<void> Bind(RenderState &pass,
                  ShaderStage stage,
                  const Shader &shader,
                  std::span<const ShaderArgument> arguments) {
  std::array<uint32_t, kBufferSizesSlot> sizes{};
  for (const auto &argument : arguments) {
    const auto slot = argument.Layout->Buffer;
    if (argument.Storage != nullptr) {
      BindBuffer(pass, stage, argument.Buffer->Native, argument.Storage->Range.Offset, slot);
      sizes[slot] = static_cast<uint32_t>(argument.Storage->Range.Bytes);
    } else if (argument.Texture != nil) {
      BindTexture(pass, stage, argument);
    } else {
      const auto bytes = argument.Constants->Bytes;
      constexpr size_t inlineBytes = 4096;
      if (bytes.size() <= inlineBytes) {
        BindBytes(pass, stage, bytes, slot);
      } else {
        const id<MTLBuffer> constants =
            [pass.Encoder.device newBufferWithBytes:bytes.data()
                                             length:bytes.size()
                                            options:MTLResourceStorageModeShared];
        if (constants == nil) {
          return Failure(ErrorKind::OutOfMemory, "Metal render uniform allocation failed");
        }
        BindBuffer(pass, stage, constants, 0, slot);
      }
    }
  }
  auto &bindings = pass.Bindings[stage == ShaderStage::Vertex ? 0u : 1u];
  if (shader.NeedsBufferSizes && (!bindings.Sizes || *bindings.Sizes != sizes)) {
    BindBytes(pass, stage, std::as_bytes(std::span(sizes)), kBufferSizesSlot);
    bindings.Sizes = sizes;
  }
  return {};
}

}

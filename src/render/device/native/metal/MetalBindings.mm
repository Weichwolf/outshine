#include "MetalInternal.h"
#include <algorithm>
#include <limits>

namespace outshine::Render::Native::Metal {

namespace {

Result<DispatchArgument>
TextureArgument(const ResourceBinding &layout, const Binding &binding, uint64_t device) {
  const auto *const sampled = std::get_if<SampledTexture>(&binding.Value);
  const auto *const storage = std::get_if<StorageTexture>(&binding.Value);
  const bool sampling = layout.Source.Kind == BindingKind::SampledTexture;
  if ((sampling && sampled == nullptr) || (!sampling && storage == nullptr)) {
    return Failure(ErrorKind::Invalid, "Metal texture binding kind differs from reflection");
  }
  const auto image = sampling ? sampled->Image : storage->Image;
  const auto *const texture = dynamic_cast<const MetalTexture *>(image.Image);
  if (texture == nullptr || texture->Device != device ||
      !Has(texture->Description().Use, sampling ? TextureUse::Sampled : TextureUse::Storage) ||
      layout.ArrayTexture != (texture->Description().Type == TextureType::Image2DArray) ||
      (layout.TexelFormat && texture->Description().Texels != *layout.TexelFormat)) {
    return Failure(ErrorKind::Invalid,
                   "Metal texture type, format, usage or ownership differs from reflection");
  }
  const auto selected = View(image, device);
  if (!selected) { return std::unexpected(selected.error()); }
  DispatchArgument argument{
      .Layout = &layout, .Buffer = {}, .Texture = *selected, .Resource = texture->Native};
  constexpr uint32_t noSampler = std::numeric_limits<uint32_t>::max();
  if (sampling && layout.Sampler != noSampler) {
    const auto *const sampler = dynamic_cast<const MetalSampler *>(sampled->Sampling);
    if (sampler == nullptr || sampler->Device != device) {
      return Failure(ErrorKind::Invalid,
                     "Metal sampled texture needs a sampler from the same device");
    }
    const auto format = texture->Description().Texels;
    if (sampler->Filtered && (format == Format::R32Float || format == Format::Rgba32Float) &&
        !texture->Native.device.supports32BitFloatFiltering) {
      return Failure(ErrorKind::Unsupported,
                     "Metal device cannot filter the requested float32 texture");
    }
    argument.Sampler = sampler->Native;
  }
  return argument;
}

}

Result<std::vector<DispatchArgument>>
Arguments(const PipelineState &pipeline, std::span<const Binding> bindings, uint64_t device) {
  if (bindings.size() != pipeline.Translated.Resources.size()) {
    return Failure(ErrorKind::Invalid, "Metal dispatch bindings differ from reflection");
  }
  std::vector<DispatchArgument> result;
  result.reserve(bindings.size());
  for (const auto &layout : pipeline.Translated.Resources) {
    const auto matches = [&](const Binding &value) {
      return value.Set == layout.Source.Set && value.Slot == layout.Source.Slot;
    };
    if (std::ranges::count_if(bindings, matches) != 1) {
      return Failure(ErrorKind::Invalid, "missing or repeated Metal dispatch binding");
    }
    const auto &binding = *std::ranges::find_if(bindings, matches);
    DispatchArgument argument{.Layout = &layout, .Buffer = {}};
    if (layout.Source.Kind == BindingKind::Uniform) {
      argument.Constants = std::get_if<Uniform>(&binding.Value);
      constexpr size_t maxUniformBytes = 64L * 1024;
      if (argument.Constants == nullptr ||
          argument.Constants->Bytes.size() < layout.Source.MinimumBytes ||
          argument.Constants->Bytes.empty() || argument.Constants->Bytes.size() > maxUniformBytes) {
        return Failure(ErrorKind::Invalid, "invalid Metal uniform byte span");
      }
    } else if (layout.Source.Kind == BindingKind::StorageBuffer) {
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
      argument.Resource = buffer->State->Native;
    } else {
      const auto texture = TextureArgument(layout, binding, device);
      if (!texture) { return std::unexpected(texture.error()); }
      argument = *texture;
    }
    result.push_back(std::move(argument));
  }
  return result;
}

Result<void> Bind(id<MTLComputeCommandEncoder> encoder,
                  const Shader &shader,
                  const std::vector<DispatchArgument> &arguments) {
  std::array<uint32_t, kBufferSizesSlot> sizes{};
  for (const auto &argument : arguments) {
    const auto slot = argument.Layout->Buffer;
    if (argument.Storage != nullptr) {
      [encoder setBuffer:argument.Buffer->Native
                  offset:argument.Storage->Range.Offset
                 atIndex:slot];
      sizes[slot] = static_cast<uint32_t>(argument.Storage->Range.Bytes);
    } else if (argument.Texture != nil) {
      [encoder setTexture:argument.Texture atIndex:argument.Layout->Texture];
      if (argument.Sampler != nil) {
        [encoder setSamplerState:argument.Sampler atIndex:argument.Layout->Sampler];
      }
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

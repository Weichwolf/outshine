#ifdef __linux__
#include "Recording.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <memory>
#include <variant>

namespace outshine::Render::Native::Vulkan {

namespace {

struct SampledBinding {
  TextureView View;
  std::shared_ptr<SamplerState> Sampler;
};

Result<SampledBinding> Sampling(const Binding &binding, const Context &context) {
  const auto *const resource = std::get_if<SampledTexture>(&binding.Value);
  const auto *const native =
      resource == nullptr ? nullptr : dynamic_cast<const VulkanSampler *>(resource->Sampling);
  if (native == nullptr || native->State()->Owner.get() != &context) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid sampled image sampler"});
  }
  return SampledBinding{.View = resource->Image, .Sampler = native->State()};
}

}

Result<VkDescriptorImageInfo> Recording::BindImage(const ShaderBinding &declared,
                                                   const Binding &binding,
                                                   const PipelineState &pipeline,
                                                   VkPipelineStageFlags2 stages) {
  TextureView view;
  std::shared_ptr<SamplerState> sampler;
  const bool storage = declared.Kind == BindingKind::StorageTexture;
  if (storage) {
    const auto *const resource = std::get_if<StorageTexture>(&binding.Value);
    if (resource == nullptr || resource->Image.Mips != 1) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "storage binding needs a single-mip image"});
    }
    view = resource->Image;
  } else {
    const auto sampled = Sampling(binding, *Context_);
    if (!sampled) { return std::unexpected(sampled.error()); }
    view = sampled->View;
    sampler = sampled->Sampler;
  }
  if (view.Image == nullptr) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = "missing image binding"});
  }
  const auto image = ImageFrom(*view.Image, *Context_);
  if (!image) { return std::unexpected(image.error()); }
  const auto expected =
      std::ranges::find_if(pipeline.Images, [&declared](const ImageRequirement &needed) {
        return needed.Set == declared.Set && needed.Slot == declared.Slot;
      });
  const auto use = storage ? TextureUse::Storage : TextureUse::Sampled;
  if (expected == pipeline.Images.end() ||
      expected->Array != ((*image)->Description.Type == TextureType::Image2DArray) ||
      (storage && expected->StorageFormat != TexelFormat((*image)->Description.Texels)) ||
      (static_cast<uint32_t>((*image)->Description.Use) & static_cast<uint32_t>(use)) == 0 ||
      !Vulkan::Supports(
          *Context_, (*image)->Description.Texels, use, sampler && sampler->Filtered)) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "image binding type, format or filtering mismatch"});
  }
  VkAccessFlags2 access = VK_ACCESS_2_SHADER_READ_BIT;
  if (storage && declared.Usage != Access::Read) { access |= VK_ACCESS_2_SHADER_WRITE_BIT; }
  if (storage && declared.Usage == Access::Write) { access = VK_ACCESS_2_SHADER_WRITE_BIT; }
  if (auto used = UseImage(
          *image, view, {.Stage = stages, .Access = access, .Layout = VK_IMAGE_LAYOUT_GENERAL});
      !used) {
    return std::unexpected(used.error());
  }
  const auto nativeView = CreateView(**image, view);
  if (!nativeView) { return std::unexpected(nativeView.error()); }
  Views_.push_back(*nativeView);
  if (sampler && std::ranges::find(Samplers_, sampler) == Samplers_.end()) {
    Samplers_.push_back(sampler);
  }
  return VkDescriptorImageInfo{.sampler = sampler ? sampler->Handle : VK_NULL_HANDLE,
                               .imageView = *nativeView,
                               .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
}

}
#endif

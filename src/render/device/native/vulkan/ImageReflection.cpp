#ifdef __linux__
#include "Reflection.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::Render::Native::Vulkan {
namespace {

VkFormat StorageFormat(SpvImageFormat format) {
  switch (format) {
    case SpvImageFormatRgba32f: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case SpvImageFormatRgba16f: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case SpvImageFormatR32f: return VK_FORMAT_R32_SFLOAT;
    case SpvImageFormatRg16f: return VK_FORMAT_R16G16_SFLOAT;
    case SpvImageFormatRgba8: return VK_FORMAT_R8G8B8A8_UNORM;
    case SpvImageFormatR8: return VK_FORMAT_R8_UNORM;
    default: return VK_FORMAT_UNDEFINED;
  }
}

Result<ImageRequirement>
ImageType(const Reflection &reflection, const spvc_reflected_resource &resource, bool storage) {
  const auto *const type = spvc_compiler_get_type_handle(reflection.Compiler, resource.type_id);
  const auto *const sampled =
      spvc_compiler_get_type_handle(reflection.Compiler, spvc_type_get_image_sampled_type(type));
  if (spvc_type_get_num_array_dimensions(type) != 0 ||
      spvc_type_get_image_dimension(type) != SpvDim2D || spvc_type_get_image_is_depth(type) != 0 ||
      spvc_type_get_image_multisampled(type) != 0 ||
      spvc_type_get_basetype(sampled) != SPVC_BASETYPE_FP32) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported,
              .Message = "native images require non-shadow float 2D/2D-array bindings"});
  }
  const auto format =
      storage ? StorageFormat(spvc_type_get_image_storage_format(type)) : VK_FORMAT_UNDEFINED;
  if (storage && format == VK_FORMAT_UNDEFINED) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "unsupported storage image format"});
  }
  return ImageRequirement{
      .Set = spvc_compiler_get_decoration(
          reflection.Compiler, resource.id, SpvDecorationDescriptorSet),
      .Slot = spvc_compiler_get_decoration(reflection.Compiler, resource.id, SpvDecorationBinding),
      .Array = spvc_type_get_image_arrayed(type) != 0,
      .StorageFormat = format};
}

}

Result<void>
AppendImages(const Reflection &reflection, spvc_resource_type kind, ComputeReflection &into) {
  const spvc_reflected_resource *resources = nullptr;
  std::size_t count = 0;
  if (spvc_resources_get_resource_list_for_type(reflection.Resources, kind, &resources, &count) !=
      SPVC_SUCCESS) {
    return std::unexpected(ReflectionError(reflection));
  }
  const bool storage = kind == SPVC_RESOURCE_TYPE_STORAGE_IMAGE;
  for (const auto &resource : std::span(resources, count)) {
    auto image = ImageType(reflection, resource, storage);
    if (!image) { return std::unexpected(image.error()); }
    auto access = storage ? Access::ReadWrite : Access::Read;
    if (storage && spvc_compiler_has_decoration(
                       reflection.Compiler, resource.id, SpvDecorationNonWritable) != 0) {
      access = Access::Read;
    }
    if (storage && spvc_compiler_has_decoration(
                       reflection.Compiler, resource.id, SpvDecorationNonReadable) != 0) {
      access = Access::Write;
    }
    into.Layout.Bindings.push_back(
        {.Set = image->Set,
         .Slot = image->Slot,
         .Kind = storage ? BindingKind::StorageTexture : BindingKind::SampledTexture,
         .Usage = access});
    into.Images.push_back(*image);
  }
  return {};
}

}
#endif

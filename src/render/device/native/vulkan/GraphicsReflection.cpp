#ifdef __linux__
#include "VulkanGraphics.h"
#include "Reflection.h"
#include <algorithm>
#include <expected>
#include <utility>
#include <cstddef>
#include <span>
#include <vector>

namespace outshine::Render::Native::Vulkan {
namespace {

Result<void> VertexInputs(const Reflection &reflection, const GraphicsDescription &description) {
  const spvc_reflected_resource *resources = nullptr;
  std::size_t count = 0;
  if (spvc_resources_get_resource_list_for_type(
          reflection.Resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, &resources, &count) !=
      SPVC_SUCCESS) {
    return std::unexpected(ReflectionError(reflection));
  }
  for (const auto &resource : std::span(resources, count)) {
    const auto location =
        spvc_compiler_get_decoration(reflection.Compiler, resource.id, SpvDecorationLocation);
    const auto attribute =
        std::ranges::find(description.Attributes, location, &VertexAttribute::Location);
    const auto *const type = spvc_compiler_get_type_handle(reflection.Compiler, resource.type_id);
    if (attribute == description.Attributes.end() || spvc_type_get_columns(type) != 1 ||
        spvc_type_get_num_array_dimensions(type) != 0 || spvc_type_get_bit_width(type) != 32) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "vertex shader input is not described"});
    }
    const auto base = spvc_type_get_basetype(type);
    const auto width = spvc_type_get_vector_size(type);
    const bool matches = attribute->Format == VertexFormat::Uint32
                             ? base == SPVC_BASETYPE_UINT32 && width == 1
                             : base == SPVC_BASETYPE_FP32 &&
                                   width * sizeof(float) == AttributeBytes(attribute->Format);
    if (!matches) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "vertex shader input format mismatch"});
    }
  }
  return {};
}

Result<void>
MergeStage(GraphicsReflection &into, const ShaderResources &reflected, ShaderStage stage) {
  for (const auto &binding : reflected.Bindings) {
    if (binding.Usage != Access::Read) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Unsupported,
                .Message = "graphics storage writes require a pass synchronization contract"});
    }
    auto existing = std::ranges::find_if(into.Layout.Bindings, [&](const GraphicsBinding &item) {
      return item.Resource.Set == binding.Set && item.Resource.Slot == binding.Slot;
    });
    if (existing == into.Layout.Bindings.end()) {
      into.Layout.Bindings.push_back({.Resource = binding, .Stages = stage});
    } else {
      if (existing->Resource.Kind != binding.Kind) {
        return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                     .Message = "graphics stages disagree on descriptor type"});
      }
      existing->Resource.MinimumBytes =
          std::max(existing->Resource.MinimumBytes, binding.MinimumBytes);
      existing->Stages = existing->Stages | UseMask(stage);
    }
  }
  for (const auto &image : reflected.Images) {
    const auto existing = std::ranges::find_if(into.Images, [&](const ImageRequirement &item) {
      return item.Set == image.Set && item.Slot == image.Slot;
    });
    if (existing == into.Images.end()) {
      into.Images.push_back(image);
    } else if (existing->Array != image.Array || existing->StorageFormat != image.StorageFormat) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "graphics stages disagree on image type"});
    }
  }
  return {};
}

Result<void> AppendStage(GraphicsReflection &into,
                         const Context &context,
                         const GraphicsDescription &description,
                         ShaderCode code,
                         ShaderStage stage) {
  const bool vertex = stage == ShaderStage::Vertex;
  Reflection reflection;
  auto parsed = ParseReflection(reflection,
                                code.Spirv,
                                code.Entry,
                                vertex ? SpvExecutionModelVertex : SpvExecutionModelFragment);
  if (!parsed) { return parsed; }
  if (vertex) {
    if (auto valid = VertexInputs(reflection, description); !valid) { return valid; }
  }
  auto resources = ReadResources(reflection);
  if (!resources) { return std::unexpected(resources.error()); }
  if (auto valid = CheckBindings(context, resources->Bindings, resources->Images); !valid) {
    return valid;
  }
  return MergeStage(into, *resources, stage);
}

}

Result<GraphicsReflection> ReflectGraphics(const Context &context,
                                           const GraphicsDescription &description) {
  GraphicsReflection result;
  if (auto vertex =
          AppendStage(result, context, description, description.Vertex, ShaderStage::Vertex);
      !vertex) {
    return std::unexpected(vertex.error());
  }
  if (description.Fragment) {
    if (auto fragment =
            AppendStage(result, context, description, *description.Fragment, ShaderStage::Fragment);
        !fragment) {
      return std::unexpected(fragment.error());
    }
  }
  std::ranges::sort(result.Layout.Bindings, [](const GraphicsBinding &a, const GraphicsBinding &b) {
    return std::pair(a.Resource.Set, a.Resource.Slot) < std::pair(b.Resource.Set, b.Resource.Slot);
  });
  std::vector<ShaderBinding> merged;
  merged.reserve(result.Layout.Bindings.size());
  for (const auto &binding : result.Layout.Bindings) { merged.push_back(binding.Resource); }
  if (auto valid = CheckBindings(context, merged, result.Images, false); !valid) {
    return std::unexpected(valid.error());
  }
  return result;
}

}
#endif

#ifdef __linux__
#include "Reflection.h"
#include <utility>
#include <string_view>
#include <span>
#include <expected>
#include <spirv_cross/spirv_cross_c.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Render::Native::Vulkan {

Error ReflectionError(const Reflection &reflection) {
  const auto *const text = reflection.Owner == nullptr
                               ? "cannot create SPIRV-Cross context"
                               : spvc_context_get_last_error_string(reflection.Owner);
  return {.Kind = ErrorKind::Invalid, .Message = text == nullptr ? "invalid SPIR-V" : text};
}

namespace {

Result<void>
ParseReflection(Reflection &reflection, std::span<const uint32_t> spirv, std::string_view entry) {
  constexpr uint32_t magic = 0x07230203u;
  constexpr std::size_t headerWords = 5;
  if (spirv.size() < headerWords || spirv.front() != magic || entry.empty() ||
      entry.find('\0') != std::string_view::npos) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid compute SPIR-V or entry point"});
  }
  if (spvc_context_create(&reflection.Owner) != SPVC_SUCCESS) {
    return std::unexpected(ReflectionError(reflection));
  }
  spvc_parsed_ir ir = nullptr;
  if (spvc_context_parse_spirv(reflection.Owner, spirv.data(), spirv.size(), &ir) != SPVC_SUCCESS ||
      spvc_context_create_compiler(reflection.Owner,
                                   SPVC_BACKEND_NONE,
                                   ir,
                                   SPVC_CAPTURE_MODE_TAKE_OWNERSHIP,
                                   &reflection.Compiler) != SPVC_SUCCESS) {
    return std::unexpected(ReflectionError(reflection));
  }
  const std::string name(entry);
  if (spvc_compiler_set_entry_point(
          reflection.Compiler, name.c_str(), SpvExecutionModelGLCompute) != SPVC_SUCCESS ||
      spvc_compiler_create_shader_resources(reflection.Compiler, &reflection.Resources) !=
          SPVC_SUCCESS) {
    return std::unexpected(ReflectionError(reflection));
  }
  constexpr std::array unsupported = {SPVC_RESOURCE_TYPE_SEPARATE_IMAGE,
                                      SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS,
                                      SPVC_RESOURCE_TYPE_PUSH_CONSTANT,
                                      SPVC_RESOURCE_TYPE_SUBPASS_INPUT,
                                      SPVC_RESOURCE_TYPE_ATOMIC_COUNTER,
                                      SPVC_RESOURCE_TYPE_ACCELERATION_STRUCTURE};
  for (const auto kind : unsupported) {
    const spvc_reflected_resource *resources = nullptr;
    std::size_t count = 0;
    if (spvc_resources_get_resource_list_for_type(reflection.Resources, kind, &resources, &count) !=
        SPVC_SUCCESS) {
      return std::unexpected(ReflectionError(reflection));
    }
    if (count != 0) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Unsupported, .Message = "unsupported compute resource kind"});
    }
  }
  return {};
}

Result<Access> BufferAccess(const Reflection &reflection, spvc_variable_id resource) {
  const SpvDecoration *decorations = nullptr;
  std::size_t count = 0;
  if (spvc_compiler_get_buffer_block_decorations(
          reflection.Compiler, resource, &decorations, &count) != SPVC_SUCCESS) {
    return std::unexpected(ReflectionError(reflection));
  }
  const std::span flags(decorations, count);
  if (std::ranges::find(flags, SpvDecorationNonWritable) != flags.end()) { return Access::Read; }
  if (std::ranges::find(flags, SpvDecorationNonReadable) != flags.end()) { return Access::Write; }
  return Access::ReadWrite;
}

Result<void> AppendBindings(const Reflection &reflection,
                            spvc_resource_type resourceType,
                            ComputeLayout &layout) {
  const spvc_reflected_resource *resources = nullptr;
  std::size_t count = 0;
  if (spvc_resources_get_resource_list_for_type(
          reflection.Resources, resourceType, &resources, &count) != SPVC_SUCCESS) {
    return std::unexpected(ReflectionError(reflection));
  }
  const bool uniform = resourceType == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER;
  for (const auto &resource : std::span(resources, count)) {
    const auto *const type = spvc_compiler_get_type_handle(reflection.Compiler, resource.type_id);
    if (spvc_type_get_num_array_dimensions(type) != 0) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Unsupported,
                .Message = "descriptor arrays need an explicit native contract"});
    }
    std::size_t bytes = 0;
    const auto *const block =
        spvc_compiler_get_type_handle(reflection.Compiler, resource.base_type_id);
    if (spvc_compiler_get_declared_struct_size(reflection.Compiler, block, &bytes) !=
        SPVC_SUCCESS) {
      return std::unexpected(ReflectionError(reflection));
    }
    const auto usage =
        uniform ? Result<Access>(Access::Read) : BufferAccess(reflection, resource.id);
    if (!usage) { return std::unexpected(usage.error()); }
    layout.Bindings.push_back({.Set = spvc_compiler_get_decoration(
                                   reflection.Compiler, resource.id, SpvDecorationDescriptorSet),
                               .Slot = spvc_compiler_get_decoration(
                                   reflection.Compiler, resource.id, SpvDecorationBinding),
                               .Kind = uniform ? BindingKind::Uniform : BindingKind::StorageBuffer,
                               .Usage = *usage,
                               .MinimumBytes = bytes});
  }
  return {};
}

}

Result<ComputeReflection> ReflectCompute(std::span<const uint32_t> spirv, std::string_view entry) {
  Reflection reflection;
  if (auto parsed = ParseReflection(reflection, spirv, entry); !parsed) {
    return std::unexpected(parsed.error());
  }
  ComputeReflection reflected;
  auto &layout = reflected.Layout;
  for (unsigned at = 0; at < layout.LocalSize.size(); ++at) {
    layout.LocalSize[at] = spvc_compiler_get_execution_mode_argument_by_index(
        reflection.Compiler, SpvExecutionModeLocalSize, at);
    if (layout.LocalSize[at] == 0) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "compute needs a fixed nonzero local size"});
    }
  }
  if (auto result = AppendBindings(reflection, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, layout);
      !result) {
    return std::unexpected(result.error());
  }
  if (auto result = AppendBindings(reflection, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, layout);
      !result) {
    return std::unexpected(result.error());
  }
  for (const auto kind : {SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, SPVC_RESOURCE_TYPE_STORAGE_IMAGE}) {
    if (auto result = AppendImages(reflection, kind, reflected); !result) {
      return std::unexpected(result.error());
    }
  }
  std::ranges::sort(layout.Bindings, [](const ShaderBinding &left, const ShaderBinding &right) {
    return std::pair(left.Set, left.Slot) < std::pair(right.Set, right.Slot);
  });
  for (std::size_t at = 1; at < layout.Bindings.size(); ++at) {
    if (layout.Bindings[at - 1].Set == layout.Bindings[at].Set &&
        layout.Bindings[at - 1].Slot == layout.Bindings[at].Slot) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "duplicate compute binding"});
    }
  }
  return reflected;
}

}
#endif

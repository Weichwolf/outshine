#include "MetalShader.h"
#include <spirv_cross_c.h>
#include <algorithm>
#include <memory>

namespace outshine::Render::Native::Metal {
namespace {

Result<Access> BufferAccess(spvc_compiler compiler, spvc_variable_id id) {
  const SpvDecoration *decorations = nullptr;
  size_t count = 0;
  if (spvc_compiler_get_buffer_block_decorations(compiler, id, &decorations, &count) !=
      SPVC_SUCCESS) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "SPIR-V buffer decorations are invalid"});
  }
  const std::span flags(decorations, count);
  if (std::ranges::find(flags, SpvDecorationNonWritable) != flags.end()) { return Access::Read; }
  if (std::ranges::find(flags, SpvDecorationNonReadable) != flags.end()) { return Access::Write; }
  return Access::ReadWrite;
}

Result<void> Reflect(spvc_compiler compiler,
                     spvc_resources resources,
                     spvc_resource_type type,
                     BindingKind kind,
                     Shader &shader) {
  const spvc_reflected_resource *items = nullptr;
  size_t count = 0;
  if (spvc_resources_get_resource_list_for_type(resources, type, &items, &count) != SPVC_SUCCESS) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "SPIR-V resource reflection failed"});
  }
  for (size_t i = 0; i < count; ++i) {
    const auto &item = items[i];
    ShaderBinding binding{
        .Set = spvc_compiler_get_decoration(compiler, item.id, SpvDecorationDescriptorSet),
        .Slot = spvc_compiler_get_decoration(compiler, item.id, SpvDecorationBinding),
        .Kind = kind,
        .Usage = Access::Read};
    if (kind == BindingKind::StorageBuffer) {
      const auto access = BufferAccess(compiler, item.id);
      if (!access) { return std::unexpected(access.error()); }
      binding.Usage = *access;
    }
    if (kind == BindingKind::Uniform || kind == BindingKind::StorageBuffer) {
      size_t bytes = 0;
      if (spvc_compiler_get_declared_struct_size(
              compiler, spvc_compiler_get_type_handle(compiler, item.base_type_id), &bytes) !=
          SPVC_SUCCESS) {
        return std::unexpected(
            Error{.Kind = ErrorKind::Invalid, .Message = "SPIR-V buffer extent is invalid"});
      }
      binding.MinimumBytes = bytes;
    }
    shader.Layout.Bindings.push_back(binding);
    shader.Resources.push_back(
        {.Source = binding,
         .Buffer = spvc_compiler_msl_get_automatic_resource_binding(compiler, item.id),
         .Texture = spvc_compiler_msl_get_automatic_resource_binding(compiler, item.id),
         .Sampler = spvc_compiler_msl_get_automatic_resource_binding_secondary(compiler, item.id)});
  }
  return {};
}

}

Result<Shader> Translate(std::span<const uint32_t> spirv, std::string_view entry) {
  constexpr size_t headerWords = 5;
  constexpr uint32_t magic = 0x07230203u;
  if (spirv.size() < headerWords || spirv.front() != magic || entry.empty() ||
      entry.contains('\0')) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid SPIR-V or entry point"});
  }
  spvc_context raw = nullptr;
  if (spvc_context_create(&raw) != SPVC_SUCCESS) {
    return std::unexpected(
        Error{.Kind = ErrorKind::OutOfMemory, .Message = "SPIRV-Cross context creation failed"});
  }
  const std::unique_ptr<spvc_context_s, decltype(&spvc_context_destroy)> context(
      raw, spvc_context_destroy);
  const auto failure = [&] {
    const char *const message = spvc_context_get_last_error_string(raw);
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = message != nullptr ? message : "SPIRV-Cross failed"});
  };
  spvc_parsed_ir parsed = nullptr;
  spvc_compiler compiler = nullptr;
  spvc_compiler_options options = nullptr;
  if (spvc_context_parse_spirv(raw, spirv.data(), spirv.size(), &parsed) != SPVC_SUCCESS ||
      spvc_context_create_compiler(
          raw, SPVC_BACKEND_MSL, parsed, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) !=
          SPVC_SUCCESS) {
    return failure();
  }
  const auto model = spvc_compiler_get_execution_model(compiler);
  const std::string name(entry);
  if (spvc_compiler_set_entry_point(compiler, name.c_str(), model) != SPVC_SUCCESS ||
      spvc_compiler_create_compiler_options(compiler, &options) != SPVC_SUCCESS ||
      spvc_compiler_options_set_uint(options,
                                     SPVC_COMPILER_OPTION_MSL_VERSION,
                                     SPVC_MAKE_MSL_VERSION(2, 3, 0)) != SPVC_SUCCESS ||
      spvc_compiler_options_set_uint(options,
                                     SPVC_COMPILER_OPTION_MSL_BUFFER_SIZE_BUFFER_INDEX,
                                     kBufferSizesSlot) != SPVC_SUCCESS ||
      spvc_compiler_install_compiler_options(compiler, options) != SPVC_SUCCESS) {
    return failure();
  }
  const char *source = nullptr;
  if (spvc_compiler_compile(compiler, &source) != SPVC_SUCCESS) { return failure(); }
  Shader shader{.Source = source,
                .Entry = spvc_compiler_get_cleansed_entry_point_name(compiler, name.c_str(), model),
                .NeedsBufferSizes =
                    static_cast<bool>(spvc_compiler_msl_needs_buffer_size_buffer(compiler))};
  if (model == SpvExecutionModelGLCompute) {
    for (uint32_t axis = 0; axis < shader.Layout.LocalSize.size(); ++axis) {
      shader.Layout.LocalSize[axis] = spvc_compiler_get_execution_mode_argument_by_index(
          compiler, SpvExecutionModeLocalSize, axis);
    }
  }
  spvc_resources resources = nullptr;
  if (spvc_compiler_create_shader_resources(compiler, &resources) != SPVC_SUCCESS) {
    return failure();
  }
  for (const auto &[type, kind] :
       {std::pair{SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, BindingKind::Uniform},
        std::pair{SPVC_RESOURCE_TYPE_STORAGE_BUFFER, BindingKind::StorageBuffer},
        std::pair{SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, BindingKind::SampledTexture},
        std::pair{SPVC_RESOURCE_TYPE_STORAGE_IMAGE, BindingKind::StorageTexture}}) {
    const auto reflected = Reflect(compiler, resources, type, kind, shader);
    if (!reflected) { return std::unexpected(reflected.error()); }
  }
  for (const auto &binding : shader.Resources) {
    if ((binding.Source.Kind == BindingKind::Uniform ||
         binding.Source.Kind == BindingKind::StorageBuffer) &&
        binding.Buffer >= kBufferSizesSlot) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "MSL buffer slots exceed the reserved layout"});
    }
  }
  return shader;
}

}

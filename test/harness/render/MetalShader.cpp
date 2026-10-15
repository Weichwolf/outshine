#include "MetalShader.h"

#include <spirv_cross_c.h>
#include <fstream>
#include <memory>

namespace {

bool Bindings(spvc_compiler compiler,
              spvc_resources resources,
              spvc_resource_type type,
              std::vector<MetalBinding> &result) {
  const spvc_reflected_resource *items = nullptr;
  size_t count = 0;
  if (spvc_resources_get_resource_list_for_type(resources, type, &items, &count) != SPVC_SUCCESS) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    result.push_back(
        {.Set = spvc_compiler_get_decoration(compiler, items[i].id, SpvDecorationDescriptorSet),
         .Binding = spvc_compiler_get_decoration(compiler, items[i].id, SpvDecorationBinding),
         .Slot = spvc_compiler_msl_get_automatic_resource_binding(compiler, items[i].id),
         .Secondary =
             spvc_compiler_msl_get_automatic_resource_binding_secondary(compiler, items[i].id)});
  }
  return true;
}

}

std::expected<MetalShader, std::string> TranslateMetal(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  constexpr std::streamoff maxBytes = 16L * 1024 * 1024;
  constexpr std::streamoff headerBytes = 5 * sizeof(uint32_t);
  constexpr uint32_t magic = 0x07230203u;
  const std::streamoff bytes = input ? input.tellg() : std::streampos(-1);
  if (bytes < headerBytes || bytes > maxBytes || bytes % 4 != 0) {
    return std::unexpected("invalid SPIR-V file size");
  }
  std::vector<uint32_t> words(static_cast<size_t>(bytes) / sizeof(uint32_t));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(words.data()), bytes);
  if (!input || words.front() != magic) { return std::unexpected("unreadable SPIR-V header"); }
  spvc_context raw = nullptr;
  if (spvc_context_create(&raw) != SPVC_SUCCESS) {
    return std::unexpected("SPIRV-Cross context creation failed");
  }
  const std::unique_ptr<spvc_context_s, decltype(&spvc_context_destroy)> context(
      raw, spvc_context_destroy);
  const auto error = [&] {
    const char *const message = spvc_context_get_last_error_string(raw);
    return std::unexpected(std::string(message != nullptr ? message : "SPIRV-Cross failed"));
  };
  spvc_parsed_ir parsed = nullptr;
  spvc_compiler compiler = nullptr;
  spvc_compiler_options options = nullptr;
  if (spvc_context_parse_spirv(raw, words.data(), words.size(), &parsed) != SPVC_SUCCESS ||
      spvc_context_create_compiler(
          raw, SPVC_BACKEND_MSL, parsed, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) !=
          SPVC_SUCCESS ||
      spvc_compiler_create_compiler_options(compiler, &options) != SPVC_SUCCESS ||
      spvc_compiler_options_set_uint(options,
                                     SPVC_COMPILER_OPTION_MSL_VERSION,
                                     SPVC_MAKE_MSL_VERSION(2, 3, 0)) != SPVC_SUCCESS ||
      spvc_compiler_install_compiler_options(compiler, options) != SPVC_SUCCESS) {
    return error();
  }
  const char *source = nullptr;
  if (spvc_compiler_compile(compiler, &source) != SPVC_SUCCESS) { return error(); }
  MetalShader result{.Source = source,
                     .Entry = spvc_compiler_get_cleansed_entry_point_name(
                         compiler, "main", spvc_compiler_get_execution_model(compiler)),
                     .Buffers = {},
                     .Textures = {}};
  spvc_resources resources = nullptr;
  if (spvc_compiler_create_shader_resources(compiler, &resources) != SPVC_SUCCESS ||
      !Bindings(compiler, resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, result.Buffers) ||
      !Bindings(compiler, resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, result.Buffers) ||
      !Bindings(compiler, resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, result.Textures)) {
    return error();
  }
  return result;
}

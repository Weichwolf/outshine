#ifndef OUTSHINE_RENDER_DEVICE_SPIRVFILE_H
#define OUTSHINE_RENDER_DEVICE_SPIRVFILE_H

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Render {

[[nodiscard]] std::expected<std::vector<uint32_t>, std::string> ReadSpirv(std::string_view path);
[[nodiscard]] std::expected<std::vector<uint32_t>, std::string>
ReadShaderSpirv(std::string_view name);

}
#endif

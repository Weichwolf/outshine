#ifndef OUTSHINE_BASE_FORMAT_CSSCOLOUR_H
#define OUTSHINE_BASE_FORMAT_CSSCOLOUR_H

#include <cstdint>
#include <optional>
#include <string_view>

namespace outshine {
[[nodiscard]] std::optional<uint32_t> ParseCssColour(std::string_view text) noexcept;
}
#endif

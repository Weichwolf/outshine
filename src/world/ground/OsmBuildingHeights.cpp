#include "OsmBuildingHeights.h"

#include <charconv>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <cmath>
#include <string_view>
#include <system_error>

namespace outshine::Ground {
namespace {

constexpr double kMetresPerFoot = 0.3048;

std::string_view Trim(std::string_view value) noexcept {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) { return {}; }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

OsmHeightValue Parse(std::string_view text, bool metric) noexcept {
  text = Trim(text);
  if (text.empty()) { return std::unexpected(OsmHeightError::InvalidNumber); }
  double value = 0.0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || !std::isfinite(value) || value < 0.0) {
    return std::unexpected(OsmHeightError::InvalidNumber);
  }
  const auto unit = Trim(text.substr(static_cast<size_t>(parsed.ptr - text.data())));
  if (!unit.empty()) {
    if (!metric) { return std::unexpected(OsmHeightError::InvalidUnit); }
    if (unit == "ft" || unit == "feet" || unit == "'") {
      value *= kMetresPerFoot;
    } else if (unit != "m") {
      return std::unexpected(OsmHeightError::InvalidUnit);
    }
  }
  return std::optional<double>{value};
}

OsmHeightValue
ReadValue(std::span<const Data::OsmTag> tags, std::string_view key, bool metric) noexcept {
  const Data::OsmTag *found = nullptr;
  for (const auto &tag : tags) {
    if (tag.Key != key) { continue; }
    if (found != nullptr) { return std::unexpected(OsmHeightError::DuplicateTag); }
    found = &tag;
  }
  return found != nullptr ? Parse(found->Value, metric) : OsmHeightValue{std::nullopt};
}

}

OsmBuildingHeights OsmBuildingHeights::Read(std::span<const Data::OsmTag> tags) noexcept {
  return {.TopM = ReadValue(tags, "height", true),
          .MinimumM = ReadValue(tags, "min_height", true),
          .Levels = ReadValue(tags, "building:levels", false),
          .MinimumLevel = ReadValue(tags, "building:min_level", false)};
}

}

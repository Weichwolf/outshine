#include "OsmBuildingHeights.h"

#include <charconv>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <cmath>
#include <string_view>
#include <system_error>

namespace outshine::Generators::Osm {
namespace {

constexpr double kMetresPerFoot = 0.3048;

outshine::Ground::BuildingHeightOrigin Origin(bool metric, bool levels) noexcept {
  if (metric) { return outshine::Ground::BuildingHeightOrigin::Declared; }
  return levels ? outshine::Ground::BuildingHeightOrigin::Storeys
                : outshine::Ground::BuildingHeightOrigin::Generated;
}

std::string_view Trim(std::string_view value) noexcept {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) { return {}; }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

HeightValue Parse(std::string_view text, bool metric) noexcept {
  text = Trim(text);
  if (text.empty()) { return std::unexpected(HeightError::InvalidNumber); }
  double value = 0.0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || !std::isfinite(value) || value < 0.0) {
    return std::unexpected(HeightError::InvalidNumber);
  }
  const auto unit = Trim(text.substr(static_cast<size_t>(parsed.ptr - text.data())));
  if (!unit.empty()) {
    if (!metric) { return std::unexpected(HeightError::InvalidUnit); }
    if (unit == "ft" || unit == "feet" || unit == "'") {
      value *= kMetresPerFoot;
    } else if (unit != "m") {
      return std::unexpected(HeightError::InvalidUnit);
    }
  }
  return std::optional<double>{value};
}

HeightValue
ReadValue(std::span<const Data::OsmTag> tags, std::string_view key, bool metric) noexcept {
  const Data::OsmTag *found = nullptr;
  for (const auto &tag : tags) {
    if (tag.Key != key) { continue; }
    if (found != nullptr) { return std::unexpected(HeightError::DuplicateTag); }
    found = &tag;
  }
  return found != nullptr ? Parse(found->Value, metric) : HeightValue{std::nullopt};
}

}

BuildingHeights BuildingHeights::Read(std::span<const Data::OsmTag> tags) noexcept {
  return {.TopM = ReadValue(tags, "height", true),
          .MinimumM = ReadValue(tags, "min_height", true),
          .Levels = ReadValue(tags, "building:levels", false),
          .MinimumLevel = ReadValue(tags, "building:min_level", false)};
}

std::expected<outshine::Ground::BuildingHeightInterval, HeightError>
BuildingHeights::Resolve(HeightPolicy policy) const noexcept {
  for (const auto &value : {TopM, MinimumM, Levels, MinimumLevel}) {
    if (!value) { return std::unexpected(value.error()); }
    if (*value && (!std::isfinite(**value) || **value < 0.0)) {
      return std::unexpected(HeightError::InvalidNumber);
    }
  }
  if (!std::isfinite(policy.StoreyHeightM) || policy.StoreyHeightM <= 0.0 ||
      !std::isfinite(policy.BodyHeightM) || policy.BodyHeightM <= 0.0) {
    return std::unexpected(HeightError::InvalidPolicy);
  }
  const double minimum = MinimumM->value_or(MinimumLevel->value_or(0.0) * policy.StoreyHeightM);
  const double levelTop = Levels->value_or(0.0) * policy.StoreyHeightM;
  const bool conflict =
      Levels->has_value() &&
      (levelTop <= minimum || (MinimumLevel->has_value() && **Levels <= **MinimumLevel));
  const bool useLevels = Levels->has_value() && !conflict;
  const double top = TopM->value_or(useLevels ? levelTop : minimum + policy.BodyHeightM);
  if (!std::isfinite(minimum) || !std::isfinite(top) || top <= minimum) {
    return std::unexpected(HeightError::InvalidInterval);
  }
  return outshine::Ground::BuildingHeightInterval{
      .TopM = top,
      .MinimumM = minimum,
      .TopOrigin = Origin(TopM->has_value(), useLevels),
      .MinimumOrigin = Origin(MinimumM->has_value(), MinimumLevel->has_value()),
      .ConflictingLevels = conflict};
}

}

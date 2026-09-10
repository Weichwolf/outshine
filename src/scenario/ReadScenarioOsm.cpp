#include "ReadScenarioOsm.h"
#include "OsmValidation.h"
#include <charconv>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <scenario/Scenario.h>
#include "Xml.h"

namespace outshine {
namespace Says {
constexpr auto kInvalidOsmLevel =
    "OSM level requires a decimal integer within the signed int range";
}

namespace {
constexpr std::string_view kWhitespace = " \t\r\n";

template <typename Number>
[[nodiscard]] std::expected<Number, std::string_view>
FeatureNumber(const Xml::Ref &node, const char *attribute, std::string_view diagnostic) {
  const auto text = node.Said(attribute);
  if (!text) { return Number{}; }
  Number value{};
  const auto parsed = std::from_chars(text->data(), text->data() + text->size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size()) {
    return std::unexpected(diagnostic);
  }
  return value;
}

[[nodiscard]] std::expected<double, std::string_view> Coordinate(std::string_view text) {
  double value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    return std::unexpected(Says::kInvalidOsmCoordinate);
  }
  return value;
}

[[nodiscard]] std::expected<std::vector<double>, std::string_view>
Coordinates(std::string_view text) {
  std::vector<double> coordinates;
  while (true) {
    const size_t start = text.find_first_not_of(kWhitespace);
    if (start == std::string_view::npos) { break; }
    text.remove_prefix(start);
    const size_t end = text.find_first_of(kWhitespace);
    const std::string_view pair = text.substr(0, end);
    const size_t comma = pair.find(',');
    if (comma == std::string_view::npos || comma == 0 || comma + 1 == pair.size()) {
      return std::unexpected(Says::kInvalidOsmCoordinate);
    }
    if (coordinates.size() == kMaxOsmPoints * 2) { return std::unexpected(Says::kOsmPointBudget); }
    const auto latitude = Coordinate(pair.substr(0, comma));
    const auto longitude = Coordinate(pair.substr(comma + 1));
    if (!latitude) { return std::unexpected(latitude.error()); }
    if (!longitude) { return std::unexpected(longitude.error()); }
    coordinates.push_back(*latitude);
    coordinates.push_back(*longitude);
    if (end == std::string_view::npos) { break; }
    text.remove_prefix(end);
  }
  return coordinates;
}
}

std::expected<void, std::string_view> ReadScenarioOsm(const Xml::Ref &osm,
                                                      std::vector<Scenario::Structure> &into) {
  for (const bool area : {false, true}) {
    const auto nodes = area ? osm.Children("area") : osm.Children("way");
    for (const Xml::Ref node : nodes) {
      Scenario::Structure made;
      made.Kind = node.Said("kind").value_or("");
      const auto width = FeatureNumber<double>(node, "widthM", Says::kInvalidOsmDimension);
      const auto height = FeatureNumber<double>(node, "heightM", Says::kInvalidOsmDimension);
      const auto level = FeatureNumber<int>(node, "level", Says::kInvalidOsmLevel);
      if (!width) { return std::unexpected(width.error()); }
      if (!height) { return std::unexpected(height.error()); }
      if (!level) { return std::unexpected(level.error()); }
      made.WidthM = *width;
      made.HeightM = *height;
      made.Area = area;
      made.Bridge = node.Said("bridge").value_or("no") == "yes";
      made.Tunnel = node.Said("tunnel").value_or("no") == "yes";
      made.Level = *level;
      const std::string text = node.Said("points").value_or("");
      auto points = Coordinates(text);
      if (!points) { return std::unexpected(points.error()); }
      made.LatLon = std::move(*points);
      const auto valid = ValidateOsmStructure(made);
      if (!valid) { return std::unexpected(valid.error()); }
      into.push_back(std::move(made));
    }
  }
  return {};
}
}

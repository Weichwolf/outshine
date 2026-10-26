#include "OsmFootpathSource.h"
#include "OsmField.h"
#include "StreetField.h"
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <string_view>

namespace outshine::Generators::Osm {
namespace {
std::expected<bool, FootpathSourceError>
Access(const OsmField &field, const OsmField::Feature &feature, const char *key) {
  const double number = field.Num(feature, key, std::numeric_limits<double>::quiet_NaN());
  if (std::isfinite(number)) {
    if (number == 0 || number == 1) { return number == 1; }
    return std::unexpected(FootpathSourceError::InvalidAccess);
  }
  const std::string_view text = field.Str(feature, key);
  if (text == "yes" || text == "true" || text == "1" || text == "designated" ||
      text == "permissive" || text == "destination") {
    return true;
  }
  if (text == "no" || text == "false" || text == "0" || text == "private") { return false; }
  return std::unexpected(FootpathSourceError::InvalidAccess);
}
}

std::expected<FootpathSourceFacts, FootpathSourceError>
ReadFootpathSource(const OsmField &field, const StreetField &ways, size_t way) {
  const auto source = ways.SourceOf(field, way);
  if (!source) { return std::unexpected(FootpathSourceError::InvalidSource); }
  const auto placement = ReadSurfacePlacement(field, source->Feature);
  if (!placement) { return std::unexpected(FootpathSourceError::InvalidPlacement); }
  const auto &feature = field.Features()[source->Feature];
  const auto &street = ways.Ways()[way];
  std::optional<bool> foot;
  const char *key = field.Has(feature, "foot") ? "foot" : "access";
  if (field.Has(feature, key)) {
    const auto access = Access(field, feature, key);
    if (!access) { return std::unexpected(access.error()); }
    foot = *access;
  }
  return FootpathSourceFacts{.Placement = *placement,
                             .Kind = field.Str(feature, "kind"),
                             .HalfWidthM = street.HalfWidthM,
                             .FootAccess = foot,
                             .OwnsEnds = street.OwnsEnds};
}
}

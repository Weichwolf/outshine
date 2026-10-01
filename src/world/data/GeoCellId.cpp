#include <world/data/GeoCellId.h>

#include <cstdint>
#include <optional>
#include <world/SourceProvider.h>

namespace outshine::Data {

namespace {
constexpr double kLongitudeLimitDeg = 180.0;
constexpr double kLatitudeLimitDeg = 90.0;
}

std::optional<SourceCoverage> GeoCellId::Bounds() const noexcept {
  if (!Valid()) { return std::nullopt; }
  const auto cells = static_cast<double>(uint32_t{1} << static_cast<unsigned>(Level));
  const double longitude = 2.0 * kLongitudeLimitDeg / cells;
  const double latitude = 2.0 * kLatitudeLimitDeg / cells;
  return SourceCoverage{.WestDeg = -kLongitudeLimitDeg + static_cast<double>(X) * longitude,
                        .SouthDeg = -kLatitudeLimitDeg + static_cast<double>(Y) * latitude,
                        .EastDeg = -kLongitudeLimitDeg + static_cast<double>(X + 1) * longitude,
                        .NorthDeg = -kLatitudeLimitDeg + static_cast<double>(Y + 1) * latitude};
}

}

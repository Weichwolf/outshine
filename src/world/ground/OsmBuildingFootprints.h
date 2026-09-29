#ifndef OUTSHINE_WORLD_GROUND_OSMBUILDINGFOOTPRINTS_H
#define OUTSHINE_WORLD_GROUND_OSMBUILDINGFOOTPRINTS_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "OsmSourceSnapshot.h"
#include "GeographicRing.h"
#include "OsmBuildingHeights.h"

namespace outshine::Ground {

enum class OsmFootprintErrorCode : uint8_t {
  MissingSource,
  MissingReference,
  AmbiguousTag,
  UnsupportedGeometry,
  InvalidRing,
  AmbiguousJunction,
  PointBudgetExceeded
};

struct OsmFootprintError {
  OsmFootprintErrorCode Code;
  Data::OsmElementId Source{};
  std::optional<Data::MissingOsmReference> Missing = std::nullopt;
};

class OsmBuildingFootprints {
public:
  using Ring = GeographicRing;

  struct Building {
    Data::OsmElementId Source;
    size_t FirstRing = 0;
    size_t RingCount = 0;
  };

  [[nodiscard]] static std::expected<OsmBuildingFootprints, OsmFootprintError>
  Build(std::shared_ptr<const Data::OsmSourceSnapshot> source, size_t maxPoints);

  [[nodiscard]] const Data::OsmSourceSnapshot &Source() const noexcept { return *Source_; }

  [[nodiscard]] std::span<const Building> Buildings() const noexcept { return Buildings_; }

  [[nodiscard]] std::span<const Ring> Rings() const noexcept { return Rings_; }

  [[nodiscard]] std::span<const double> Points() const noexcept { return Points_; }

  [[nodiscard]] std::span<const Data::OsmTag> Tags(const Building &building) const noexcept;

  [[nodiscard]] OsmBuildingHeights Heights(const Building &building) const noexcept {
    return OsmBuildingHeights::Read(Tags(building));
  }

private:
  OsmBuildingFootprints() = default;

  [[nodiscard]] std::expected<void, OsmFootprintError> AppendRing(std::span<const uint64_t> nodes,
                                                                  Data::OsmElementId source,
                                                                  bool exterior,
                                                                  size_t maxPoints);
  [[nodiscard]] std::expected<void, OsmFootprintError>
  AppendRelation(const Data::OsmRelation &relation, size_t maxPoints);

  [[nodiscard]] std::expected<void, OsmFootprintError>
  AppendWays(const std::vector<const Data::OsmWay *> &ways,
             Data::OsmElementId source,
             bool exterior,
             size_t maxPoints);

  std::shared_ptr<const Data::OsmSourceSnapshot> Source_;
  std::vector<Building> Buildings_;
  std::vector<Ring> Rings_;
  std::vector<double> Points_;
};

}

#endif

#ifndef OUTSHINE_GENERATORS_BUILDING_HIPROOFPLAN_H
#define OUTSHINE_GENERATORS_BUILDING_HIPROOFPLAN_H

#include "Earth.h"

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators {

enum class HipRoofError {
  InvalidInput,
  CapacityExceeded,
  PartitionFailed,
  InvalidFacet,
  InsufficientClearance
};

class HipRoofPlan {
public:
  struct Limits {
    size_t Vertices = 256;
    size_t Nodes = 1024;
    size_t Facets = 256;
    size_t Corners = 8192;
  };

  struct Facet {
    std::vector<EastNorth> Ring;
    EastNorth Gradient;
    double InterceptM = 0;
    size_t SourceRing = 0;
    size_t SourceEdge = 0;

    [[nodiscard]] double DistanceAt(EastNorth point) const noexcept;
    [[nodiscard]] bool Contains(EastNorth point, double tolerance) const noexcept;
  };

  [[nodiscard]] static std::expected<HipRoofPlan, HipRoofError>
  Prepare(std::span<const EastNorth> exterior,
          std::span<const std::vector<EastNorth>> holes,
          Limits limits);

  [[nodiscard]] EastNorth Origin() const noexcept { return Origin_; }

  [[nodiscard]] double MaximumDistanceM() const noexcept { return MaximumDistanceM_; }

  [[nodiscard]] double PrecisionM() const noexcept { return PrecisionM_; }

  [[nodiscard]] std::span<const Facet> Facets() const noexcept { return Facets_; }

  [[nodiscard]] std::optional<double> DistanceAt(EastNorth point) const noexcept;

private:
  friend class HipRoofCodec;
  [[nodiscard]] bool OwnsEveryEave(size_t count) const;
  EastNorth Origin_;
  double MaximumDistanceM_ = 0;
  double PrecisionM_ = 0;
  std::vector<Facet> Facets_;
};

}
#endif

#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANAREADOMAIN_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANAREADOMAIN_H

#include "UrbanArea.h"
#include "OsmField.h"
#include "GeosGeometry.h"
#include <geos_c.h>
#include <memory>
#include <string_view>

namespace outshine::Generators::Osm::UrbanDetail {
struct OwnerFrame {
  std::array<double, 4> Bounds{};
  double Scale = 0;
};

struct Domain {
  GEOSContextHandle_t Context = GEOS_init_r();
  Spatial::GeosGeometry Allowed{nullptr, {Context}};
  const GEOSPreparedGeometry *Prepared = nullptr;
  std::array<double, 4> Bounds{};
  double Scale = 0;
  std::vector<UrbanSourceUse> Sources;
  bool HasUrbanEvidence = false;

  Domain() = default;
  Domain(const Domain &) = delete;
  Domain &operator=(const Domain &) = delete;
  ~Domain();

  [[nodiscard]] Spatial::GeosGeometry Own(GEOSGeometry *value) const { return {value, {Context}}; }

  [[nodiscard]] std::expected<bool, std::string> Covers(std::array<double, 4> bounds) const;
};

[[nodiscard]] bool Expired(const UrbanAreaLimits &limits);
[[nodiscard]] OwnerFrame FrameFor(const OsmField::Tile &owner);
[[nodiscard]] std::expected<std::unique_ptr<Domain>, std::string> BuildDomain(
    std::span<const OsmField *const> inputs, OwnerFrame frame, const UrbanAreaLimits &limits);
[[nodiscard]] Spatial::GeosGeometry
SourceGeometry(const Domain &domain, const OsmField &field, const OsmField::Feature &feature);
[[nodiscard]] Spatial::GeosGeometry
Collect(const Domain &domain, std::vector<Spatial::GeosGeometry> parts, int type);
}
#endif

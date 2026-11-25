#include "FootprintTerraces.h"
#include "GeosPolygon.h"
#include "TangentFrame.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <geos_c.h>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr unsigned kTileMetres = 256, kSamplesPerEdge = 4, kHeightStepM = 2;
constexpr unsigned kSamplesPerCell = kSamplesPerEdge * kSamplesPerEdge;
constexpr auto kNoRoof = std::numeric_limits<int16_t>::min();
constexpr auto kNoFloor = std::numeric_limits<int16_t>::max();

struct GeosContext {
  GEOSContextHandle_t Handle = GEOS_init_r();

  ~GeosContext() {
    if (Handle != nullptr) { GEOS_finish_r(Handle); }
  }
};

struct ReleasePrepared {
  GEOSContextHandle_t Context;

  void operator()(const GEOSPreparedGeometry *geometry) const {
    GEOSPreparedGeom_destroy_r(Context, geometry);
  }
};

struct Bounds {
  static constexpr double Infinity = std::numeric_limits<double>::infinity();
  std::array<double, 2> Least{Infinity, Infinity}, Most{-Infinity, -Infinity};
};

struct Heights {
  int16_t Bottom = kNoFloor, Top = kNoRoof;
};

struct Grid {
  std::array<double, 2> Origin{};
  unsigned Columns = 0, Rows = 0;
  std::vector<Heights> Samples;
};

bool Extend(Bounds &bounds, const TerraceFootprint &footprint) {
  if (footprint.Rings.empty() || !std::isfinite(footprint.BottomM) ||
      !std::isfinite(footprint.TopM) || footprint.TopM <= footprint.BottomM) {
    return false;
  }
  for (const auto &ring : footprint.Rings) {
    if (ring.size() < 3 || ring.size() >= std::numeric_limits<unsigned>::max()) { return false; }
    for (const auto &point : ring) {
      for (size_t axis = 0; axis < point.size(); ++axis) {
        if (!std::isfinite(point[axis])) { return false; }
        bounds.Least[axis] = std::min(bounds.Least[axis], point[axis]);
        bounds.Most[axis] = std::max(bounds.Most[axis], point[axis]);
      }
    }
  }
  return true;
}

std::expected<Grid, std::string> Layout(const Bounds &bounds, const TerraceRequest &request) {
  constexpr unsigned cellsMost = std::numeric_limits<unsigned>::max() / kSamplesPerEdge;
  Grid grid;
  std::array<unsigned, 2> dimensions{};
  for (size_t axis = 0; axis < dimensions.size(); ++axis) {
    const double origin = std::floor(bounds.Least[axis] / request.SpacingM);
    const double count = std::ceil(bounds.Most[axis] / request.SpacingM) - origin;
    if (!std::isfinite(count) || count <= 0 || count > cellsMost) {
      return std::unexpected("terrace extent is not representable");
    }
    grid.Origin[axis] = origin * request.SpacingM;
    dimensions[axis] = static_cast<unsigned>(count) * kSamplesPerEdge;
  }
  grid.Columns = dimensions[0];
  grid.Rows = dimensions[1];
  if (grid.Rows > request.SamplesMost / grid.Columns) {
    return std::unexpected("terrace sample budget exceeded");
  }
  grid.Samples.resize(static_cast<size_t>(grid.Columns) * grid.Rows);
  return grid;
}

std::expected<void, std::string> Rasterize(Grid &grid,
                                           GEOSContextHandle_t context,
                                           const TerraceFootprint &footprint,
                                           const Bounds &bounds,
                                           const TerraceRequest &request,
                                           size_t &testsLeft) {
  const double step = static_cast<double>(request.SpacingM) / kSamplesPerEdge;
  const auto first = [&](size_t axis, unsigned most) {
    return static_cast<unsigned>(
        std::clamp(std::floor((bounds.Least[axis] - grid.Origin[axis]) / step),
                   0.,
                   static_cast<double>(most)));
  };
  const auto last = [&](size_t axis, unsigned most) {
    return static_cast<unsigned>(std::clamp(
        std::ceil((bounds.Most[axis] - grid.Origin[axis]) / step), 0., static_cast<double>(most)));
  };
  const unsigned left = first(0, grid.Columns);
  const unsigned top = first(1, grid.Rows);
  const unsigned right = last(0, grid.Columns);
  const unsigned bottom = last(1, grid.Rows);
  if (left >= right || top >= bottom) { return {}; }
  const uint64_t tests = uint64_t{right - left} * (bottom - top);
  if (tests > testsLeft) { return std::unexpected("terrace point-test budget exceeded"); }
  testsLeft -= static_cast<size_t>(tests);
  const double upper = std::nearbyint(footprint.TopM / kHeightStepM);
  const double lower = std::min(upper - 1, std::nearbyint(footprint.BottomM / kHeightStepM));
  if (lower <= kNoRoof || upper >= kNoFloor) {
    return std::unexpected("terrace height exceeds encoding range");
  }
  const Heights heights{.Bottom = static_cast<int16_t>(lower), .Top = static_cast<int16_t>(upper)};
  const auto polygon = Spatial::MakeGeosPolygon(context, footprint.Rings);
  if (!polygon) { return std::unexpected("could not construct terrace footprint"); }
  const std::unique_ptr<const GEOSPreparedGeometry, ReleasePrepared> prepared(
      GEOSPrepare_r(context, polygon.get()), ReleasePrepared{context});
  if (!prepared) { return std::unexpected("could not prepare terrace footprint"); }
  for (unsigned row = top; row < bottom; ++row) {
    if (request.Stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
    const double z = grid.Origin[1] + (row + .5) * step;
    for (unsigned column = left; column < right; ++column) {
      const double x = grid.Origin[0] + (column + .5) * step;
      const auto hit = GEOSPreparedContainsXY_r(context, prepared.get(), x, z);
      if (hit == 0) { continue; }
      if (hit != 1) { return std::unexpected("could not sample terrace footprint"); }
      auto &sample = grid.Samples[static_cast<size_t>(row) * grid.Columns + column];
      sample.Bottom = std::min(sample.Bottom, heights.Bottom);
      sample.Top = std::max(sample.Top, heights.Top);
    }
  }
  return {};
}

PrismSpan Cell(const Grid &grid, unsigned row, unsigned column) {
  unsigned occupied = 0;
  Heights combined;
  for (unsigned y = 0; y < kSamplesPerEdge; ++y) {
    const size_t first = (static_cast<size_t>(row) * kSamplesPerEdge + y) * grid.Columns +
                         static_cast<size_t>(column) * kSamplesPerEdge;
    for (unsigned x = 0; x < kSamplesPerEdge; ++x) {
      const auto &value = grid.Samples[first + x];
      occupied += static_cast<unsigned>(value.Top != kNoRoof);
      combined.Top = std::max(combined.Top, value.Top);
      combined.Bottom = std::min(combined.Bottom, value.Bottom);
    }
  }
  if (occupied < kSamplesPerCell / 2) { return {}; }
  return {.Lower = static_cast<float>(combined.Bottom) * kHeightStepM,
          .Upper = static_cast<float>(combined.Top) * kHeightStepM};
}

Mat4 Placement(const Grid &grid, const TerraceRequest &request, unsigned row, unsigned column) {
  const auto frame = TangentFrame::At(request.Anchor);
  Mat4 result;
  const std::array axes{frame.EastEcef(), frame.UpEcef(), frame.NorthEcef() * -1.};
  for (size_t axis = 0; axis < axes.size(); ++axis) {
    for (size_t component = 0; component < axes[axis].Axis.size(); ++component) {
      result.At(component, axis) = axes[axis][component];
    }
  }
  result.SetTranslation(frame.ToEcefPosition(
      {.EastM = grid.Origin[0] + static_cast<double>(column) * request.SpacingM,
       .NorthM = -grid.Origin[1] - static_cast<double>(row) * request.SpacingM}));
  return result;
}

std::expected<UrbanPrisms, std::string> Pack(const Grid &grid, const TerraceRequest &request) {
  UrbanPrisms result;
  auto samples = std::make_shared<std::vector<PrismSpan>>();
  const unsigned rows = grid.Rows / kSamplesPerEdge;
  const unsigned columns = grid.Columns / kSamplesPerEdge;
  const unsigned side = kTileMetres / request.SpacingM;
  for (unsigned row = 0; row < rows; row += side) {
    for (unsigned column = 0; column < columns; column += side) {
      if (request.Stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
      const unsigned high = std::min(rows - row, side);
      const unsigned wide = std::min(columns - column, side);
      const size_t first = samples->size();
      bool occupied = false;
      for (unsigned y = 0; y < high; ++y) {
        for (unsigned x = 0; x < wide; ++x) {
          const auto cell = Cell(grid, row + y, column + x);
          samples->push_back(cell);
          occupied = occupied || cell.Upper > cell.Lower;
        }
      }
      if (!occupied) {
        samples->resize(first);
        continue;
      }
      const auto spacing = static_cast<float>(request.SpacingM);
      result.Geometry.Patches.push_back({.FirstSample = first,
                                         .Columns = wide,
                                         .Rows = high,
                                         .PitchX = spacing,
                                         .PitchZ = spacing});
      result.Placements.push_back({.EcefFromLocal = Placement(grid, request, row, column)});
    }
  }
  result.Geometry.Samples = std::move(samples);
  return result;
}
}

std::expected<UrbanPrisms, std::string>
BuildFootprintTerraces(std::span<const TerraceFootprint> footprints,
                       const TerraceRequest &request) {
  if (footprints.empty() || request.SpacingM == 0 || kTileMetres % request.SpacingM != 0 ||
      !(std::abs(request.Anchor.LatitudeDeg) <= kDegPerHalfTurn / 2) ||
      !(std::abs(request.Anchor.LongitudeDeg) <= kDegPerHalfTurn)) {
    return std::unexpected("invalid footprint terrace request");
  }
  Bounds domain;
  std::vector<Bounds> bounds(footprints.size());
  for (size_t at = 0; at < footprints.size(); ++at) {
    if (request.Stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
    if (!Extend(bounds[at], footprints[at])) {
      return std::unexpected("invalid source footprint terrace polygon");
    }
    for (size_t axis = 0; axis < domain.Least.size(); ++axis) {
      domain.Least[axis] = std::min(domain.Least[axis], bounds[at].Least[axis]);
      domain.Most[axis] = std::max(domain.Most[axis], bounds[at].Most[axis]);
    }
  }
  auto grid = Layout(domain, request);
  if (!grid) { return std::unexpected(grid.error()); }
  const GeosContext context;
  if (context.Handle == nullptr) {
    return std::unexpected("could not initialize terrace rasterizer");
  }
  size_t testsLeft = request.PointTestsMost;
  for (size_t at = 0; at < footprints.size(); ++at) {
    const auto ready =
        Rasterize(*grid, context.Handle, footprints[at], bounds[at], request, testsLeft);
    if (!ready) { return std::unexpected(ready.error()); }
  }
  return Pack(*grid, request);
}
}

#include "TerrainResidency.h"

#include "ChunkSurface.h"
#include "GroundLattice.h"
#include "TileGeodesy.h"
#include "math/RenderFrame.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace outshine {
namespace {
constexpr size_t kOcclusionProbes = 64;
constexpr int kSide = Render::GroundLattice::kSide;
constexpr int kPageSide = kSide + 2;

struct CellCoordinate {
  int Node;
  double Fraction;
};

CellCoordinate CellAt(double fraction, uint32_t postings, bool uniform) noexcept {
  if (uniform) {
    const double at = std::clamp(fraction, 0.0, 1.0) * (kSide - 1);
    const int node = std::min(static_cast<int>(at), kSide - 2);
    return {.Node = node, .Fraction = at - node};
  }
  const double at = std::clamp(fraction, 0.0, 1.0) * (postings - 1);
  const int node = Ground::ChunkNodeCell(at, {.Side = kSide, .Postings = postings});
  const double low = Ground::ChunkNodePosting(node, postings, kSide);
  const double high = Ground::ChunkNodePosting(node + 1, postings, kSide);
  return {.Node = node, .Fraction = high > low ? (at - low) / (high - low) : 0.0};
}
}

std::optional<double> TerrainResidency::HeightMAt(LongitudeLatitude at) const noexcept {
  const Ground::Geo geo{.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg};
  if (!Ground::TileIndex::Of(geo, 0).Tile()) { return std::nullopt; }
  const Ground::TileFrac base = Ground::ToTileFracClamped(geo, 0);
  for (int zoom = static_cast<int>(kZoomLevels) - 1; zoom >= 0; --zoom) {
    const double span = std::ldexp(1.0, zoom);
    const double x = base.X * span;
    const double y = base.Y * span;
    const Data::TileId tile{.Zoom = zoom,
                            .X = static_cast<uint32_t>(std::clamp(std::floor(x), 0.0, span - 1)),
                            .Y = static_cast<uint32_t>(std::clamp(std::floor(y), 0.0, span - 1))};
    const size_t *found = PageIndex_.Find(tile);
    if (found == nullptr) { continue; }
    const Held &page = Held_[*found];
    if (!page.Virtual && page.Postings < 2) { continue; }
    const CellCoordinate col = CellAt(x - tile.X, page.Postings, page.Virtual);
    const CellCoordinate row = CellAt(y - tile.Y, page.Postings, page.Virtual);
    const size_t first =
        (static_cast<size_t>(row.Node) + 1) * kPageSide + static_cast<size_t>(col.Node) + 1;
    const double nw = page.Nodes[first];
    const double ne = page.Nodes[first + 1];
    const double sw = page.Nodes[first + kPageSide];
    const double se = page.Nodes[first + kPageSide + 1];
    if (col.Fraction >= row.Fraction) {
      return nw + col.Fraction * (ne - nw) + row.Fraction * (se - ne);
    }
    return nw + row.Fraction * (sw - nw) + col.Fraction * (se - sw);
  }
  return std::nullopt;
}

bool TerrainResidency::Occludes(const Ray &ray,
                                float nearM,
                                float distanceM,
                                const TangentFrame &frame) const noexcept {
  if (Held_.empty() || nearM < 0 || distanceM <= nearM || !std::isfinite(nearM) ||
      !std::isfinite(distanceM)) {
    return false;
  }
  const auto ecefDirection = [&](const Vec3f &local) {
    return frame.EastEcef() * static_cast<double>(local[0]) +
           frame.UpEcef() * static_cast<double>(local[1]) +
           frame.NorthEcef() * RenderFrame::NorthOfZ(local[2]);
  };
  const Vec3 origin = frame.OriginEcef() + ecefDirection(ray.OriginM);
  const Vec3 toward = ecefDirection(ray.Toward);
  for (size_t probe = 0; probe < kOcclusionProbes; ++probe) {
    const double fraction = (static_cast<double>(probe) + 0.5) / kOcclusionProbes;
    const Vec3 position = origin + toward * std::lerp(nearM, distanceM, fraction);
    const Ground::Geo geo =
        Ground::EcefToGeoWgs84({.X = position[0], .Y = position[1], .Z = position[2]});
    const auto heightM =
        HeightMAt({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg});
    if (heightM && *heightM > geo.HeightM) { return true; }
  }
  return false;
}
}

#include "BuildingCanopy.h"
#include "BuildingPreparation.h"
#include "BuildingShape.h"
#include "BuildingSurface.h"
#include "PreparedStructureTile.h"
#include "StructureMesher.h"
#include "TangentFrame.h"
#include "Vec2.h"
#include "Vec3.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr double kPostHalfM = .25;
constexpr double kInsetM = .60;
constexpr double kContactM = .03;
constexpr double kMinimumCornerSine = .03;

Vec2 Planar(EastNorth p) noexcept {
  return {{p.EastM, p.NorthM}};
}

double Cross(Vec2 a, Vec2 b) noexcept {
  return a[0] * b[1] - a[1] * b[0];
}

double Dot2(Vec2 a, Vec2 b) noexcept {
  return a[0] * b[0] + a[1] * b[1];
}

std::optional<Vec2> InsetCorner(std::span<const EastNorth, 4> ring, size_t i) noexcept {
  const Vec2 centre = Planar(ring[i]);
  const Vec2 before = Planar(ring[(i + 3) % 4]) - centre;
  const Vec2 after = Planar(ring[(i + 1) % 4]) - centre;
  const double scaled = std::abs(Cross(before, after));
  const double first = std::hypot(before[0], before[1]);
  const double second = std::hypot(after[0], after[1]);
  if (!(scaled > kMinimumCornerSine * first * second)) { return std::nullopt; }
  return centre + (before * second + after * first) * (kInsetM / scaled);
}

bool FitsCap(std::span<const EastNorth, 4> ring, Vec2 centre, double sign) noexcept {
  for (size_t i = 0; i < ring.size(); ++i) {
    const Vec2 a = Planar(ring[i]);
    const Vec2 edge = Planar(ring[(i + 1) % ring.size()]) - a;
    if (!(sign * Cross(edge, centre - a) >
          std::numbers::sqrt2 * kPostHalfM * std::hypot(edge[0], edge[1]))) {
      return false;
    }
  }
  return true;
}

std::optional<std::array<Vec2, 4>> PostCentres(const BuildingShape &cap) noexcept {
  if (cap.RoundFootprint || !cap.Holes.empty() || cap.Ring.size() != 4) { return std::nullopt; }
  const std::span<const EastNorth, 4> ring(cap.Ring.data(), 4);
  const double sign =
      Cross(Planar(ring[1]) - Planar(ring[0]), Planar(ring[2]) - Planar(ring[1])) > 0 ? 1 : -1;
  std::array<Vec2, 4> result;
  for (size_t i = 0; i < result.size(); ++i) {
    const auto centre = InsetCorner(ring, i);
    if (!centre || !FitsCap(ring, *centre, sign)) { return std::nullopt; }
    result[i] = *centre;
  }
  return result;
}

bool NearSegment(Vec2 point, Vec2 from, Vec2 to, double clearanceM) noexcept {
  const Vec2 edge = to - from;
  const double length2 = Dot2(edge, edge);
  const double along = length2 > 0 ? std::clamp(Dot2(point - from, edge) / length2, 0.0, 1.0) : 0;
  const Vec2 gap = point - (from + edge * along);
  return Dot2(gap, gap) <= clearanceM * clearanceM;
}

bool BlocksWay(std::span<const Vec2, 4> posts,
               const TangentFrame &frame,
               std::span<const WayLine> ways) noexcept {
  for (const auto &way : ways) {
    const auto points = way.LatLon;
    if (points.size() < 4) { continue; }
    const auto local = [&](size_t i) {
      return Planar(
          frame.ToLocalGroundPosition({.LongitudeDeg = points[i + 1], .LatitudeDeg = points[i]}));
    };
    const double clearanceM = way.HalfWidthM + std::numbers::sqrt2 * kPostHalfM;
    Vec2 from = local(0);
    for (size_t i = 2; i + 1 < points.size(); i += 2) {
      const Vec2 to = local(i);
      if (std::ranges::any_of(posts,
                              [&](Vec2 post) { return NearSegment(post, from, to, clearanceM); })) {
        return true;
      }
      from = to;
    }
  }
  return false;
}

constexpr std::array kPostCorners{Vec2{{-kPostHalfM, -kPostHalfM}},
                                  Vec2{{kPostHalfM, -kPostHalfM}},
                                  Vec2{{kPostHalfM, kPostHalfM}},
                                  Vec2{{-kPostHalfM, kPostHalfM}}};

struct CanopyContact {
  const BuildingSurface &Cap;
  double UndersideM;
};

std::optional<double> ContactHeight(CanopyContact contact,
                                    const BuildingSurface &roof,
                                    Vec2 post,
                                    std::vector<double> &cuts) {
  const auto &cap = contact.Cap;
  double deepest = 0;
  for (const auto corner : kPostCorners) {
    const auto p = post + corner;
    const Vec3 top = cap.Origin() + cap.Axes().East * p[0] + cap.Axes().North * p[1] +
                     cap.Axes().Up * contact.UndersideM;
    const Vec3 bottom = top - cap.Axes().Up * contact.UndersideM;
    for (size_t axis = 0; axis < 3; ++axis) {
      if (std::min(top[axis], bottom[axis]) > roof.Bounds().Max[axis] ||
          std::max(top[axis], bottom[axis]) < roof.Bounds().Min[axis]) {
        return std::nullopt;
      }
    }
    const auto hit = roof.Trace(
        {.Origin = top, .Direction = cap.Axes().Up * -1.0}, kContactM, contact.UndersideM, cuts);
    if (!hit || !roof.IsRoof(*hit)) { return std::nullopt; }
    deepest = std::max(deepest, hit->Along);
  }
  return contact.UndersideM - deepest - kContactM;
}

BuildingShape Column(Vec2 centre, double baseM, const BuildingShape &cap, double undersideM) {
  BuildingShape column;
  for (const Vec2 corner : kPostCorners) {
    const auto point = centre + corner;
    column.Ring.push_back({.EastM = point[0], .NorthM = point[1]});
  }
  column.PartyWallEdges.resize(column.Ring.size());
  column.AreaM2 = 4 * kPostHalfM * kPostHalfM;
  column.Centre = {.EastM = centre[0], .NorthM = centre[1]};
  column.AxisU = {.EastM = 1};
  column.HalfUm = column.HalfVm = kPostHalfM;
  column.Fill = 1;
  column.Form = BuildingForm::Outbuilding;
  column.OpeningStyle = FacadeStyle::Outbuilding;
  column.SeatM = column.SoleM = baseM;
  column.EavesM = undersideM + kContactM - baseM;
  column.FloorM = column.EavesM;
  column.Ident = cap.Ident;
  column.WallVariant = cap.WallVariant;
  return column;
}

std::optional<std::array<double, 4>> ContactHeights(CanopyContact contact,
                                                    const BuildingSurface &roof,
                                                    std::span<const Vec2, 4> centres,
                                                    std::vector<double> &cuts) {
  std::array<double, 4> result;
  for (size_t post = 0; post < centres.size(); ++post) {
    const auto height = ContactHeight(contact, roof, centres[post], cuts);
    if (!height) { return std::nullopt; }
    result[post] = *height;
  }
  return result;
}

std::optional<std::array<BuildingShape, 4>>
RoofSupports(const PreparedStructureTile &tile, size_t capIndex, std::span<const WayLine> ways) {
  const auto &surface = tile.Surfaces[capIndex];
  const auto parts = surface.Shapes();
  if (parts.size() != 1) { return std::nullopt; }
  const auto &cap = parts.front();
  const auto centres = PostCentres(cap);
  if (!centres) { return std::nullopt; }
  const auto &layout = tile.Structures[capIndex].Layout;
  const size_t first = size_t{2} * layout.LocalFirst;
  const auto at = TangentFrame::At(
      {.LongitudeDeg = tile.PointsLatLon[first + 1], .LatitudeDeg = tile.PointsLatLon[first]});
  if (BlocksWay(*centres, at, ways)) { return std::nullopt; }
  const double undersideM = BuildingBottomM(cap, layout.MinimumHeightM);
  const CanopyContact contact{.Cap = surface, .UndersideM = undersideM};
  std::vector<double> cuts;
  std::optional<std::array<double, 4>> supported;
  double highest = -std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < tile.Structures.size(); ++i) {
    const auto &roof = tile.Surfaces[i];
    if (tile.Structures[i].Layout.MinimumHeightM != 0 || roof.Bounds().Empty()) { continue; }
    const auto heights = ContactHeights(contact, roof, *centres, cuts);
    if (!heights) { continue; }
    const double lower = std::ranges::min(*heights);
    if (!(lower > highest)) { continue; }
    highest = lower;
    supported = heights;
  }
  if (!supported) { return std::nullopt; }
  std::array<BuildingShape, 4> result;
  for (size_t post = 0; post < centres->size(); ++post) {
    result[post] = Column((*centres)[post], (*supported)[post], cap, undersideM);
  }
  return result;
}
}

bool IsCanopySlab(CanopyHeights heights) noexcept {
  const double thicknessM = heights.TopM - heights.MinimumM;
  return heights.MinimumM >= kFloorUnsaidM && thicknessM > 0 && thicknessM <= .5 * kFloorUnsaidM;
}

bool AttachCanopySupports(PreparedStructureTile &tile,
                          std::span<const WayLine> ways,
                          const std::atomic_bool *stopping) {
  for (size_t i = 0; i < tile.Structures.size(); ++i) {
    if (stopping != nullptr && stopping->load(std::memory_order_relaxed)) { return false; }
    const auto &layout = tile.Structures[i].Layout;
    if (!IsCanopySlab({.MinimumM = layout.MinimumHeightM, .TopM = layout.HeightM})) { continue; }
    auto supports = RoofSupports(tile, i, ways);
    if (!supports) { continue; }
    for (auto &shape : *supports) { tile.Surfaces[i].AppendPart(std::move(shape)); }
  }
  return true;
}
}

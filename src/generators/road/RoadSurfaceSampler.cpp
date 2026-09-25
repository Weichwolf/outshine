#include "RoadSurfaceSampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>

#include "math/RenderFrame.h"
#include "TangentFrame.h"

namespace outshine::Generators {
namespace {

constexpr double kSmallTriangle = 1.0e-12;
constexpr double kCoverageTolerance = 0.001;
constexpr double kVerticalToleranceM = 1.0;

[[nodiscard]] Vec3 VertexAt(std::span<const float> positions, uint32_t index) {
  const size_t at = static_cast<size_t>(index) * 3u;
  return {{positions[at], positions[at + 1u], positions[at + 2u]}};
}

[[nodiscard]] std::optional<RoadSurfaceContact>
ContactOnTriangle(const RoadSurfaceSpan &span,
                  std::span<const float> positions,
                  std::span<const uint32_t> triangles,
                  size_t triangleIndex,
                  const Vec3 &at,
                  double stationM,
                  double lateralOffsetM) {
  const size_t first = triangleIndex * 3u;
  if (first + 2u >= triangles.size()) { return std::nullopt; }
  const std::array vertices{triangles[first], triangles[first + 1u], triangles[first + 2u]};
  if (std::ranges::any_of(vertices, [positions](uint32_t index) {
        return static_cast<size_t>(index) >= positions.size() / 3u;
      })) {
    return std::nullopt;
  }
  const Vec3 a = VertexAt(positions, vertices[0]);
  const Vec3 b = VertexAt(positions, vertices[1]);
  const Vec3 c = VertexAt(positions, vertices[2]);
  const double cross = (b[0] - a[0]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[0] - a[0]);
  if (!std::isfinite(cross) || std::abs(cross) <= kSmallTriangle) { return std::nullopt; }
  const double wb = ((at[0] - a[0]) * (c[2] - a[2]) - (at[2] - a[2]) * (c[0] - a[0])) / cross;
  const double wc = ((b[0] - a[0]) * (at[2] - a[2]) - (b[2] - a[2]) * (at[0] - a[0])) / cross;
  const double wa = 1.0 - wb - wc;
  if (!std::isfinite(wa) || !std::isfinite(wb) || !std::isfinite(wc) ||
      std::min({wa, wb, wc}) < -kCoverageTolerance) {
    return std::nullopt;
  }
  Vec3 normal = Cross(b - a, c - a);
  if (!Normalise(normal) || normal[1] <= 0.0) { return std::nullopt; }
  const double heightM = wa * a[1] + wb * b[1] + wc * c[1];
  if (!std::isfinite(heightM) || std::abs(heightM - at[1]) > kVerticalToleranceM) {
    return std::nullopt;
  }
  return RoadSurfaceContact{.PositionM = {{at[0], heightM, at[2]}},
                            .Normal = normal,
                            .SourceEdge = span.SourceEdge,
                            .StationM = stationM,
                            .LateralOffsetM = lateralOffsetM};
}

}

std::optional<RoadSurfaceContact> RoadSurfaceSampler::At(const RoadAlignment &alignment,
                                                         const RoadSurface &surface,
                                                         double stationM,
                                                         double lateralOffsetM) {
  if (!std::isfinite(stationM) || !std::isfinite(lateralOffsetM) || stationM < 0.0 ||
      stationM > alignment.LengthM() || surface.SourceIdentity != alignment.SourceIdentity() ||
      surface.TerrainDigest != alignment.TerrainDigest() ||
      surface.AlignmentAnchor.LongitudeDeg != alignment.Anchor().LongitudeDeg ||
      surface.AlignmentAnchor.LatitudeDeg != alignment.Anchor().LatitudeDeg) {
    return std::nullopt;
  }
  const auto pose = alignment.AtStation(stationM);
  if (!pose || !std::isfinite(pose->WidthM) || pose->WidthM <= 0.0 ||
      std::abs(lateralOffsetM) > pose->WidthM * 0.5) {
    return std::nullopt;
  }
  const auto found =
      std::ranges::upper_bound(surface.Spans, pose->StationM, {}, &RoadSurfaceSpan::StartStationM);
  if (found == surface.Spans.begin()) { return std::nullopt; }
  const size_t current = static_cast<size_t>(found - surface.Spans.begin() - 1);
  if (pose->StationM > surface.Spans[current].EndStationM) { return std::nullopt; }
  const double horizontal = std::hypot(pose->TangentEnu[0], pose->TangentEnu[1]);
  if (!std::isfinite(horizontal) || horizontal <= 0.0) { return std::nullopt; }
  const EastNorthUp across{
      .EastM = pose->PositionM.EastM - pose->TangentEnu[1] / horizontal * lateralOffsetM,
      .NorthM = pose->PositionM.NorthM + pose->TangentEnu[0] / horizontal * lateralOffsetM,
      .UpM = pose->PositionM.UpM};
  const TangentFrame alignmentFrame = TangentFrame::At(surface.AlignmentAnchor);
  const TangentFrame renderFrame = TangentFrame::At(surface.RenderAnchor);
  const Vec3 ecef = alignmentFrame.OriginEcef() + alignmentFrame.EastEcef() * across.EastM +
                    alignmentFrame.NorthEcef() * across.NorthM +
                    alignmentFrame.UpEcef() * across.UpM;
  const Vec3 at{RenderFrame::Of(renderFrame.ToLocalPosition(ecef))};
  const auto withinSpan = [&](size_t index) -> std::optional<RoadSurfaceContact> {
    const RoadSurfaceSpan &span = surface.Spans[index];
    if (span.Part < 0 || span.Part >= surface.SurfaceGeometry.parts()) { return std::nullopt; }
    const auto positions = surface.SurfaceGeometry.positionsOf(span.Part);
    const auto triangles = surface.SurfaceGeometry.trianglesOf(span.Part);
    for (size_t local = 0; local < 2u; ++local) {
      if (auto contact = ContactOnTriangle(span,
                                           positions,
                                           triangles,
                                           static_cast<size_t>(span.FirstTriangle) + local,
                                           at,
                                           pose->StationM,
                                           lateralOffsetM)) {
        return contact;
      }
    }
    return std::nullopt;
  };
  if (auto contact = withinSpan(current)) { return contact; }
  if (current > 0u) {
    if (auto contact = withinSpan(current - 1u)) { return contact; }
  }
  if (current + 1u < surface.Spans.size()) {
    if (auto contact = withinSpan(current + 1u)) { return contact; }
  }
  return std::nullopt;
}

}

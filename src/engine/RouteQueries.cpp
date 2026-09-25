#include <Outshine.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <ranges>
#include <string>
#include <string_view>

#include "EngineHeld.h"
#include "TangentFrame.h"
#include "math/RenderFrame.h"
#include "RoadSurfaceSampler.h"

namespace outshine {
namespace {

struct RouteFrames {
  const TangentFrame &Alignment;
  const TangentFrame &World;
};

[[nodiscard]] const NamedRoadAlignment *PublishedRoute(const Surrounds &world,
                                                       std::string_view name) {
  if (!world.GroundPublished.Current()) { return nullptr; }
  const auto found =
      std::ranges::find_if(world.RoadAlignments, [&](const NamedRoadAlignment &route) {
        return route.Id == name && route.Alignment;
      });
  return found == world.RoadAlignments.end() ? nullptr : &*found;
}

[[nodiscard]] Vec3 InWorldFrame(const RouteFrames &frames, EastNorthUp position) {
  const Vec3 ecef = frames.Alignment.OriginEcef() + frames.Alignment.EastEcef() * position.EastM +
                    frames.Alignment.NorthEcef() * position.NorthM +
                    frames.Alignment.UpEcef() * position.UpM;
  return {RenderFrame::Of(frames.World.ToLocalPosition(ecef))};
}

[[nodiscard]] Vec3 DirectionInWorldFrame(const RouteFrames &frames, const Vec3 &direction) {
  const Vec3 ecef = frames.Alignment.EastEcef() * direction[0] +
                    frames.Alignment.NorthEcef() * direction[1] +
                    frames.Alignment.UpEcef() * direction[2];
  return {RenderFrame::Of(frames.World.ToLocalDirection(ecef))};
}

}

Holds<RouteInfo> Engine::State::PublishedRouteInfo(std::string_view name) const {
  const auto declared =
      std::ranges::find_if(Session.Declared.Routes, [&](const Scenario::RouteDeclaration &route) {
        return route.Id == name;
      });
  if (declared == Session.Declared.Routes.end()) {
    return std::unexpected("unknown route '" + std::string(name) + "'");
  }
  const NamedRoadAlignment *published = PublishedRoute(World, name);
  if (published == nullptr) {
    return std::unexpected("route '" + std::string(name) + "' has no current published alignment");
  }
  const Generators::RoadAlignment &alignment = *published->Alignment;
  return RouteInfo{.LengthM = alignment.LengthM(),
                   .SegmentCount = alignment.Edges().size(),
                   .Closed = alignment.Closed()};
}

Holds<RoutePose> Engine::State::SamplePublishedRoute(std::string_view name, double stationM) const {
  if (!std::isfinite(stationM) || stationM < 0.0) {
    return std::unexpected("route station must be finite and nonnegative");
  }
  const auto info = PublishedRouteInfo(name);
  if (!info) { return std::unexpected(info.error()); }
  if (stationM > info->LengthM) {
    return std::unexpected("route station exceeds the published route length");
  }
  const NamedRoadAlignment *published = PublishedRoute(World, name);
  if (published == nullptr) { return std::unexpected("route alignment changed during sampling"); }
  const Generators::RoadAlignment &alignment = *published->Alignment;
  const auto pose = alignment.AtStation(stationM);
  if (!pose) { return std::unexpected("published route has no valid pose at this station"); }
  const auto upper = std::ranges::upper_bound(
      alignment.Edges(), pose->StationM, {}, &Generators::RoadAlignmentEdge::StartStationM);
  const size_t segmentIndex = upper == alignment.Edges().begin()
                                  ? 0
                                  : static_cast<size_t>(upper - alignment.Edges().begin() - 1);
  const TangentFrame alignmentFrame = TangentFrame::At(alignment.Anchor());
  const Scenario::Georeference &origin = Session.Declared.Ground.Origin;
  const TangentFrame worldFrame =
      TangentFrame::At({.LongitudeDeg = origin.LongitudeDeg, .LatitudeDeg = origin.LatitudeDeg});
  const RouteFrames frames{.Alignment = alignmentFrame, .World = worldFrame};
  Vec3 forward = DirectionInWorldFrame(frames, pose->TangentEnu);
  Vec3 up = DirectionInWorldFrame(frames, {{0.0, 0.0, 1.0}});
  if (!Normalise(forward)) {
    return std::unexpected("published route has an invalid world-frame tangent");
  }
  up = up - forward * Dot(up, forward);
  if (!Normalise(up)) {
    return std::unexpected("published route has an invalid world-frame basis");
  }
  return RoutePose{.PositionM = InWorldFrame(frames, pose->PositionM),
                   .Forward = forward,
                   .Up = up,
                   .StationM = pose->StationM,
                   .WidthM = pose->WidthM,
                   .SegmentIndex = segmentIndex};
}

Holds<RouteContact> Engine::State::SamplePublishedRouteContact(std::string_view name,
                                                               double stationM,
                                                               double lateralOffsetM) const {
  if (!std::isfinite(stationM) || !std::isfinite(lateralOffsetM) || stationM < 0.0) {
    return std::unexpected("route contact requires finite station and lateral offset");
  }
  const auto info = PublishedRouteInfo(name);
  if (!info) { return std::unexpected(info.error()); }
  if (stationM > info->LengthM) {
    return std::unexpected("route contact station exceeds the published route length");
  }
  const NamedRoadAlignment *published = PublishedRoute(World, name);
  if (published == nullptr || !published->Surface) {
    return std::unexpected("route '" + std::string(name) +
                           "' has no current published road surface");
  }
  const Generators::RoadAlignment &alignment = *published->Alignment;
  const auto contact =
      Generators::RoadSurfaceSampler::At(alignment, *published->Surface, stationM, lateralOffsetM);
  if (!contact) {
    return std::unexpected(
        "route '" + std::string(name) + "' has no matching road triangle at station " +
        std::to_string(stationM) + " m and offset " + std::to_string(lateralOffsetM) + " m");
  }
  const TangentFrame surfaceFrame = TangentFrame::At(published->Surface->RenderAnchor);
  const Scenario::Georeference &origin = Session.Declared.Ground.Origin;
  const TangentFrame worldFrame =
      TangentFrame::At({.LongitudeDeg = origin.LongitudeDeg, .LatitudeDeg = origin.LatitudeDeg});
  const RouteFrames frames{.Alignment = surfaceFrame, .World = worldFrame};
  const EastNorthUp local{.EastM = contact->PositionM[0],
                          .NorthM = -contact->PositionM[2],
                          .UpM = contact->PositionM[1]};
  const Vec3 normal = DirectionInWorldFrame(
      frames, {{contact->Normal[0], -contact->Normal[2], contact->Normal[1]}});
  const auto upper = std::ranges::upper_bound(
      alignment.Edges(), contact->StationM, {}, &Generators::RoadAlignmentEdge::StartStationM);
  const size_t segmentIndex = upper == alignment.Edges().begin()
                                  ? 0
                                  : static_cast<size_t>(upper - alignment.Edges().begin() - 1);
  return RouteContact{.PositionM = InWorldFrame(frames, local),
                      .Normal = normal,
                      .StationM = contact->StationM,
                      .LateralOffsetM = contact->LateralOffsetM,
                      .SegmentIndex = segmentIndex};
}

Holds<RouteInfo> Engine::routeInfo(std::string_view name) const {
  return S_->PublishedRouteInfo(name);
}

Holds<RoutePose> Engine::sampleRoute(std::string_view name, double stationM) const {
  return S_->SamplePublishedRoute(name, stationM);
}

Holds<RouteContact>
Engine::sampleRouteContact(std::string_view name, double stationM, double lateralOffsetM) const {
  return S_->SamplePublishedRouteContact(name, stationM, lateralOffsetM);
}

}

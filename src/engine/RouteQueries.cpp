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

namespace outshine {
namespace {

struct RouteFrames {
  const TangentFrame &Alignment;
  const TangentFrame &World;
};

[[nodiscard]] const NamedRoadAlignment *PublishedRoute(const Surrounds &world,
                                                       std::string_view name) {
  if (!world.OsmTransportLoader ||
      world.OsmTransportLoader->CurrentPhase() != World::OsmTransportLoader::Phase::Ready ||
      !world.OsmTransportLoader->Current()) {
    return nullptr;
  }
  const Data::OsmSourceIdentity &source = world.OsmTransportLoader->Current()->SourceIdentity();
  const auto found =
      std::ranges::find_if(world.RoadAlignments, [&](const NamedRoadAlignment &route) {
        return route.Id == name && route.Alignment && route.Alignment->SourceIdentity() == source;
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

Holds<RouteInfo> Engine::routeInfo(std::string_view name) const {
  const auto declared = std::ranges::find_if(
      S_->Session.Declared.Routes,
      [&](const Scenario::RouteDeclaration &route) { return route.Id == name; });
  if (declared == S_->Session.Declared.Routes.end()) {
    return std::unexpected("unknown route '" + std::string(name) + "'");
  }
  const NamedRoadAlignment *published = PublishedRoute(S_->World, name);
  if (published == nullptr) {
    return std::unexpected("route '" + std::string(name) + "' has no current published alignment");
  }
  const Generators::RoadAlignment &alignment = *published->Alignment;
  return RouteInfo{.LengthM = alignment.LengthM(),
                   .SegmentCount = alignment.Edges().size(),
                   .Closed = alignment.Closed()};
}

Holds<RoutePose> Engine::sampleRoute(std::string_view name, double stationM) const {
  if (!std::isfinite(stationM) || stationM < 0.0) {
    return std::unexpected("route station must be finite and nonnegative");
  }
  const auto info = routeInfo(name);
  if (!info) { return std::unexpected(info.error()); }
  if (stationM > info->LengthM) {
    return std::unexpected("route station exceeds the published route length");
  }
  const NamedRoadAlignment *published = PublishedRoute(S_->World, name);
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
  const Scenario::Georeference &origin = S_->Session.Declared.Ground.Origin;
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

}

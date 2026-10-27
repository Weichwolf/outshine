#include "GroundPortBodies.h"
#include "HeightSolidTerrainContact.h"
#include "TangentFrame.h"
#include "TransportNetworkSnapshot.h"
#include "Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <stop_token>
#include <utility>

namespace outshine::Generators {
namespace {
using Error = GroundPortBodyError;
template <typename T> using Checked = std::expected<T, Error>;

bool Valid(LongitudeLatitude at) {
  return std::isfinite(at.LongitudeDeg) && std::isfinite(at.LatitudeDeg) &&
         std::abs(at.LongitudeDeg) <= kDegPerHalfTurn &&
         std::abs(at.LatitudeDeg) <= kDegPerHalfTurn / 2;
}

struct FrameChange {
  EastNorthUp Origin, East, North, Up;

  FrameChange(LongitudeLatitude sourceAnchor, const TangentFrame &target) {
    const auto source = TangentFrame::At(sourceAnchor);
    Origin = target.ToLocalPosition(source.OriginEcef());
    East = target.ToLocalDirection(source.EastEcef());
    North = target.ToLocalDirection(source.NorthEcef());
    Up = target.ToLocalDirection(source.UpEcef());
  }

  [[nodiscard]] std::array<double, 3> At(const std::array<double, 3> &point) const {
    return {Origin.EastM + point[0] * East.EastM + point[1] * North.EastM + point[2] * Up.EastM,
            Origin.NorthM + point[0] * East.NorthM + point[1] * North.NorthM + point[2] * Up.NorthM,
            Origin.UpM + point[0] * East.UpM + point[1] * North.UpM + point[2] * Up.UpM};
  }
};

Checked<void> Admit(const GroundPortBodies &bodies, GroundPortBodyBudget budget) {
  if (bodies.Bodies.size() > budget.Bodies ||
      GroundPortBodyCapacityBytes(bodies) > budget.ContainerBytes) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  size_t remaining = budget.SurfaceFaces;
  for (const auto &body : bodies.Bodies) {
    const auto &geometry = body.Geometry;
    if (geometry.SurfaceFaces > remaining || geometry.Points.size() > budget.Solid.Vertices ||
        geometry.Faces.size() > budget.Solid.Faces ||
        geometry.SurfaceFaces > budget.Solid.SurfaceFaces) {
      return std::unexpected(Error{RoadGroundError::CapacityExceeded});
    }
    remaining -= geometry.SurfaceFaces;
    if (bodies.Source->Topology().FindEdge(body.Source) == nullptr ||
        !std::isfinite(body.SourceBegin) || !std::isfinite(body.SourceEnd) ||
        body.SourceBegin < 0 || body.SourceEnd > 1 || body.SourceBegin >= body.SourceEnd ||
        (!bodies.Core && (body.SourceBegin != 0 || body.SourceEnd != 1)) ||
        body.FaceParents.size() != geometry.SurfaceFaces ||
        (!bodies.Core && body.TerrainContacts.size() != geometry.SurfaceFaces)) {
      return std::unexpected(Error{HeightSolidError::InvalidInput});
    }
    for (const auto &contact : body.TerrainContacts) {
      if (contact.RingEastNorthM.size() != 6 || contact.SeamEastNorthM.size() != 6) {
        return std::unexpected(Error{HeightSolidError::InvalidInput});
      }
    }
  }
  return {};
}

Checked<EarthworkStamp> ReframeContact(EarthworkStamp &&contact,
                                       const FrameChange &change,
                                       const SurfaceGroundSource &source,
                                       double apronM) {
  std::array<std::array<double, 3>, 3> points{};
  for (size_t at = 0; at < 3; ++at) {
    const EastNorth point{.EastM = contact.RingEastNorthM[2 * at],
                          .NorthM = contact.RingEastNorthM[2 * at + 1]};
    points[at] = change.At({point.EastM, point.NorthM, contact.WantsAt(point)});
  }
  auto transformed = BuildTriangleTerrainContact(points, source, apronM, std::move(contact));
  if (!transformed) { return std::unexpected(Error{transformed.error()}); }
  return std::move(*transformed);
}

Checked<void> Reframe(GroundPortBody &body,
                      const FrameChange &change,
                      GroundPortBodyRecipe recipe,
                      bool sliced,
                      GroundPortBodyWork &work,
                      const std::stop_token &stop) {
  for (auto &point : body.Geometry.Points) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    point = change.At(point);
    if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2])) {
      return std::unexpected(Error{HeightSolidError::Unrepresentable});
    }
    ++work.ReframedVertices;
  }
  const SurfaceGroundSource source{.Effect = SurfaceGroundEffect::GroundContact,
                                   .ContactKey = body.Source.PathId,
                                   .ClearanceM = 0,
                                   .ReliefM = recipe.ReliefM};
  for (size_t face = 0; face < body.TerrainContacts.size(); ++face) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    if (sliced) {
      auto contact =
          ReframeContact(std::move(body.TerrainContacts[face]), change, source, recipe.ApronM);
      if (!contact) { return std::unexpected(contact.error()); }
      body.TerrainContacts[face] = std::move(*contact);
    } else {
      auto contact = BuildHeightSolidTerrainContact(
          body.Geometry, face, source, recipe.ApronM, std::move(body.TerrainContacts[face]));
      if (!contact) { return std::unexpected(Error{contact.error()}); }
      body.TerrainContacts[face] = std::move(*contact);
    }
    ++work.ReframedTerrainContacts;
  }
  return {};
}
}

std::expected<GroundPortBodies, GroundPortBodyError>
ReframeGroundPortBodies(GroundPortBodies &&input,
                        LongitudeLatitude anchor,
                        GroundPortBodyBudget budget,
                        const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
  if (!input.Source || !Valid(input.Anchor) || !Valid(anchor) ||
      (input.Core && !input.Core->Valid()) ||
      (input.Scope && input.Scope->Input.Revision != input.Source->SourceIdentity().Revision)) {
    return std::unexpected(Error{HeightSolidError::InvalidInput});
  }
  if (auto admitted = Admit(input, budget); !admitted) { return std::unexpected(admitted.error()); }
  auto bodies = std::move(input);
  if (bodies.Anchor == anchor) { return bodies; }
  const FrameChange change(bodies.Anchor, TangentFrame::At(anchor));
  for (auto &body : bodies.Bodies) {
    if (auto result =
            Reframe(body, change, bodies.Recipe, bodies.Core.has_value(), bodies.Work, stop);
        !result) {
      return std::unexpected(result.error());
    }
  }
  bodies.Anchor = anchor;
  bodies.Work.ContainerCapacityBytes = GroundPortBodyCapacityBytes(bodies);
  return bodies;
}
}

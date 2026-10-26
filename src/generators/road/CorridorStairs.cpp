#include "Corridors.h"
#include "OsmFootpathSource.h"
#include "RoadStepBody.h"
#include "SurfaceEarthworks.h"
#include "Capacity.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ratio>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
bool Corridors::PrepareStepBody(const Paving &on,
                                Edge &edge,
                                Paved &into,
                                std::vector<EarthworkStamp> &corridor) {
  const auto source = on.Ways.SourceOf(on.Vectors, edge.Lane);
  if (!source || on.Vectors.Str(on.Vectors.Features()[source->Feature], "kind") != "steps") {
    return false;
  }
  const auto facts = Osm::ReadFootpathSource(on.Vectors, on.Ways, edge.Lane);
  if (!facts || facts->Placement.Layer.value_or(0) != 0 ||
      facts->Placement.Bridge.value_or(false) || facts->Placement.Tunnel.value_or(false)) {
    ++into.StairRefusals[0];
    return false;
  }
  const auto began = std::chrono::steady_clock::now();
  const auto surface = SurfaceOf(
      on.Ways.Ways()[edge.Lane], on.Vegetation, edge, into.UnbankedWays.contains(edge.Lane));
  auto body = BuildRoadStepBody(into.Along, surface);
  into.StairBodyMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (!body) {
    ++into.StairRefusals[1 + static_cast<size_t>(body.error())];
    return false;
  }
  const SurfaceGroundSource contact{.Effect = SurfaceGroundEffect::GroundContact,
                                    .ContactKey = into.ContactKeys[edge.Lane]};
  const std::array contacts{contact, contact, contact};
  auto earthworks = BuildSurfaceEarthworks(*body, contacts);
  if (!earthworks) {
    ++into.StairRefusals[4];
    return false;
  }
  edge.StepBody = std::make_shared<const SurfaceBody>(std::move(*body));
  const auto &held = *edge.StepBody;
  into.StairBodyBytes += CapacityBytes(held.ReferencePoints) + CapacityBytes(held.ReferenceFaces) +
                         CapacityBytes(held.FaceSources) + CapacityBytes(held.FaceLevels) +
                         CapacityBytes(held.Levels) + CapacityBytes(held.Stairs) +
                         CapacityBytes(held.Geometry.Points) + CapacityBytes(held.Geometry.Faces) +
                         CapacityBytes(held.Geometry.WallParents) +
                         CapacityBytes(held.Geometry.WallKinds);
  for (auto &stamp : earthworks->Stamps) { corridor.push_back(std::move(stamp)); }
  ++into.StairBodies;
  return true;
}

void Corridors::RecordStepMetrics(Paved &into) {
  Notes(into, "stairs: native bodies", static_cast<double>(into.StairBodies), "bodies");
  Notes(into, "stairs: body preparation", into.StairBodyMs, "ms");
  Notes(into, "stairs: retained body arrays", static_cast<double>(into.StairBodyBytes), "bytes");
  constexpr std::array names{"stairs: unsupported placement",
                             "stairs: invalid body input",
                             "stairs: unsupported flight geometry",
                             "stairs: body construction refused",
                             "stairs: terrain contact refused"};
  for (size_t at = 0; at < names.size(); ++at) {
    Notes(into, names[at], static_cast<double>(into.StairRefusals[at]), "fallbacks");
  }
}
}

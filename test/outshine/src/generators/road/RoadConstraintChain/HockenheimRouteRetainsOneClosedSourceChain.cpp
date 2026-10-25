#include "HockenheimTransportFixture.h"
#include "HockenheimCircuitFixture.h"
#include "OsmTransportBuilder.h"
#include "Check.h"
#include "OsmXmlReader.h"
#include "RoadConstraintChain.h"

#include <cmath>
#include <cstddef>
#include <string>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr double kMinimumChordLengthM = 3000;
constexpr double kMaximumChordLengthM = 7000;
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  auto fixture = Circuit::LoadNetwork();
  CHECK(fixture,
        fixture ? "pinned source, topology and directed circuit load" : fixture.error().c_str());
  if (!fixture) { return Report(); }
  const auto &topology = fixture->Graph;
  auto &route = fixture->Route;

  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100.0f, 100.0f, 100.0f, 100.0f};
  block.Sources = {{.Kind = DataKind::Elevation,
                    .Tile = {.Zoom = 0, .X = 0, .Y = 0},
                    .SourceId = "analytic-dem",
                    .Revision = "r1"}};
  const auto heights = Ground::HeightField::Of(0, {block});
  const auto chain =
      RoadConstraintChain::Build(topology, route.SourceIdentity, route.EdgeIds, *heights);
  CHECK(chain.has_value(), "every selected route edge has a DEM-backed constraint point");
  if (!chain) { return Report(); }
  CHECK(chain->Closed() && chain->Edges().size() == Circuit::EdgeCount &&
            chain->Points().size() == Circuit::EdgeCount + 1 &&
            chain->Points().front().SourceNodeId == chain->Points().back().SourceNodeId,
        "one ordered, closed chain retains all 267 directed source edges");
  CHECK(chain->EstimatedLengthM() > kMinimumChordLengthM &&
            chain->EstimatedLengthM() < kMaximumChordLengthM &&
            std::isfinite(chain->EstimatedLengthM()),
        "the local chord chain has a plausible circuit-scale length");
  bool allRaceway = true;
  for (size_t index = 0; index < chain->Edges().size(); ++index) {
    const RoadConstraintEdge &edge = chain->Edges()[index];
    allRaceway &= edge.SourceEdge == route.EdgeIds[index] &&
                  edge.Facility == TransportFacility::Raceway &&
                  edge.Surface == TransportSurface::Asphalt &&
                  edge.WidthM == Circuit::TrackWidthM && !edge.Bridge && !edge.Tunnel;
  }
  CHECK(allRaceway, "the route retains source order and raceway profiles without pitlane edges");
  return Report();
}

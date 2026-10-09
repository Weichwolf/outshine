#include "Check.h"
#include "TerrainMesh.h"
#include "TerrainPress.h"
#include "TileGeodesy.h"
#include "TriangleBvh.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
constexpr int kSide = 33;
constexpr int kHalo = 1;
constexpr int kZoom = 20;
constexpr uint32_t kCentre = 1u << (kZoom - 1);
constexpr outshine::Generators::TerrainPageLayout kLayout{.Side = kSide, .Halo = kHalo};

outshine::Patchwork Candidate() {
  outshine::Patchwork candidate;
  const std::array tiles{
      outshine::Data::TileId{.Zoom = kZoom, .X = kCentre, .Y = kCentre},
      outshine::Data::TileId{.Zoom = kZoom + 1, .X = 2 * (kCentre + 1), .Y = 2 * kCentre},
      outshine::Data::TileId{.Zoom = kZoom + 1, .X = 2 * (kCentre + 1), .Y = 2 * kCentre + 1}};
  for (const auto tile : tiles) {
    candidate.Sheets.push_back({.Tile = tile,
                                .Nodes = std::vector<float>(kLayout.NodeCount(), 10.0f),
                                .Side = kSide,
                                .Postings = kSide,
                                .Virtual = true});
  }
  return candidate;
}

outshine::TriangleBvh Surface(const outshine::Patchwork &candidate,
                              const outshine::TangentFrame &frame) {
  const auto mesh = outshine::Generators::BuildTerrainMesh(candidate, frame, kLayout);
  return outshine::TriangleBvh::Over(mesh.PositionsM, mesh.Indices);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const TangentFrame frame = TangentFrame::At({});
  const auto geo = Ground::TileFracToGeo({.X = kCentre + 1.0, .Y = kCentre + .137}, kZoom);
  const auto centre = frame.ToLocalGroundPosition(
      {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg});
  EarthworkStamp contact;
  contact.LowE = centre.EastM - .1;
  contact.HighE = centre.EastM + .1;
  contact.LowN = centre.NorthM - .1;
  contact.HighN = centre.NorthM + .1;
  contact.AtE = centre.EastM;
  contact.AtN = centre.NorthM;
  contact.SlopeE = .07;
  contact.SlopeN = .08;
  contact.ApronM = .2;
  contact.Fills = true;
  contact.Kind = EarthworkKind::Corridor;
  contact.RingEastNorthM = {contact.LowE,
                            contact.LowN,
                            contact.HighE,
                            contact.LowN,
                            contact.HighE,
                            contact.HighN,
                            contact.LowE,
                            contact.HighN};
  Patchwork pointContact = Candidate();
  (void)PressTerrain(std::span{&contact, 1u}, pointContact, frame, kLayout, 30.0);
  const auto before =
      Surface(pointContact, frame)
          .Under(static_cast<float>(centre.EastM), static_cast<float>(-centre.NorthM));
  CHECK(before && *before > 1.0f,
        "a narrow contact between nodes leaves the terrain cell above it");
  contact.PlanarSupportM = 0.0;
  Patchwork supported = Candidate();
  const auto pressed = PressTerrain(std::span{&contact, 1u}, supported, frame, kLayout, 30.0);
  CHECK(pressed.Nodes > 0 && pressed.Held == 0 && pressed.Structures == 0,
        "bounded planar support deforms the incident cells without exceeding the height limit");
  const auto surface = Surface(supported, frame);
  for (const double east : {contact.LowE, centre.EastM, contact.HighE}) {
    for (const double north : {contact.LowN, centre.NorthM, contact.HighN}) {
      const auto up = surface.Under(static_cast<float>(east), static_cast<float>(-north));
      CHECK(up && *up <= contact.WantsAt({.EastM = east, .NorthM = north}) + .001,
            "interpolated terrain stays below the same contact plane across both resolutions");
    }
  }
  for (int row = 0; row <= 16; ++row) {
    CHECK(supported.Sheets[0].Nodes[kLayout.NodeAt(kSide - 1, row)] ==
              supported.Sheets[1].Nodes[kLayout.NodeAt(0, 2 * row)],
          "shared coarse and fine boundary nodes retain identical heights");
  }
  CHECK(supported.Sheets[0].Nodes[kLayout.NodeAt(0, kSide - 1)] == 10.0f,
        "distant terrain outside the contact and apron remains unchanged");
  Patchwork sliced = Candidate();
  TerrainPressJob job({contact}, sliced, frame, kLayout, 30.0);
  bool done = false;
  for (int step = 0; step < 10000 && !done; ++step) { done = job.Advance(1, 17); }
  CHECK(done, "sliced planar support completes with bounded work");
  for (size_t sheet = 0; sheet < sliced.Sheets.size(); ++sheet) {
    CHECK(sliced.Sheets[sheet].Nodes == supported.Sheets[sheet].Nodes,
          "sliced and synchronous pressing share one implementation");
  }
  return Report();
}

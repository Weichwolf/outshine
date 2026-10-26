#include "Check.h"
#include "SurfaceBody.h"
#include "SurfaceEarthworks.h"
#include "TerrainDeformation.h"
#include "TerrainMesh.h"
#include "TriangleBvh.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr int kSide = 9;
constexpr int kZoom = 22;
constexpr TerrainPageLayout kLayout{.Side = kSide, .Halo = 1};
constexpr double kGroundTopM = 4;
constexpr double kBridgeTopM = 14;
constexpr double kSlopeE = .07;
constexpr double kSlopeN = .03;
constexpr double kClearanceM = 4;
constexpr double kHeightToleranceM = .001;
constexpr double kEarthworkLimitM = 30;
constexpr double kExtentM = 20;
constexpr float kHighTerrainM = 20;

enum class Sheets : uint8_t { Single, Shared, Independent };

std::expected<SurfaceBody, SurfaceBodyError> Body(Sheets sheets) {
  SurfaceHeightTopology topology;
  topology.Points = {
      {-kExtentM, -kExtentM}, {kExtentM, -kExtentM}, {kExtentM, kExtentM}, {-kExtentM, kExtentM}};
  topology.Sources = {{.Faces = {{0, 1, 2}, {0, 2, 3}}, .PlanarFaces = {0, 1}}};
  if (sheets == Sheets::Shared) { topology.Sources.push_back(topology.Sources[0]); }
  if (sheets == Sheets::Independent) {
    const auto copy = topology.Points;
    topology.Points.insert(topology.Points.end(), copy.begin(), copy.end());
    topology.Sources.push_back({.Faces = {{4, 5, 6}, {4, 6, 7}}, .PlanarFaces = {2, 3}});
  }
  std::vector<SurfaceHeightRecipe> sources(topology.Sources.size());
  std::vector<double> heights;
  for (size_t at = 0; at < topology.Points.size(); ++at) {
    const auto xy = topology.Points[at];
    heights.push_back((at < 4 ? kGroundTopM : kBridgeTopM) + kSlopeE * xy[0] + kSlopeN * xy[1]);
  }
  const SurfaceHeightRequest request{.Topology = topology,
                                     .Sources = sources,
                                     .Ports = {},
                                     .Flights = {},
                                     .Contacts = {},
                                     .Orders = {},
                                     .MaximumAdjustmentM = 0,
                                     .MaximumCrossfall = 0};
  return BuildSurfaceBody(request, heights);
}

Patchwork Candidate(float height) {
  constexpr uint32_t centre = uint32_t{1} << static_cast<unsigned>(kZoom - 1);
  Patchwork result;
  result.Sheets.push_back({.Tile = {.Zoom = kZoom, .X = centre, .Y = centre},
                           .Nodes = std::vector<float>(kLayout.NodeCount(), height),
                           .Side = kSide,
                           .Postings = kSide,
                           .Virtual = true});
  return result;
}

void CheckGroundMesh(const Patchwork &candidate, double base) {
  const auto mesh = BuildTerrainMesh(candidate, TangentFrame::At({}), kLayout);
  const auto surface = TriangleBvh::Over(mesh.PositionsM, mesh.Indices);
  for (const float east : {1.0f, 4.0f, 8.0f}) {
    for (const float north : {-1.0f, -4.0f, -8.0f}) {
      const auto height = surface.Under(east, -north);
      CHECK(height.has_value(), "deformed DEM mesh covers the requested contact point");
      if (!height) { continue; }
      CHECK_NEAR(*height,
                 base + kSlopeE * east + kSlopeN * north,
                 kHeightToleranceM,
                 "m",
                 "actual terrain triangles follow the same body underside");
    }
  }
}

void GroundAndIndependentOverheadHaveDifferentEffects() {
  const auto body = Body(Sheets::Independent);
  CHECK(body.has_value(), "two independent closed sheets provide the same contact input");
  if (!body) { return; }
  const std::array sources{SurfaceGroundSource{.Effect = SurfaceGroundEffect::GroundContact,
                                               .ContactKey = 1,
                                               .ReliefM = 5},
                           SurfaceGroundSource{.Effect = SurfaceGroundEffect::OverheadClearance,
                                               .ClearanceM = kClearanceM,
                                               .ReliefM = 5}};
  const auto contacts = BuildSurfaceEarthworks(*body, sources, {.ApronM = 0});
  CHECK(contacts && contacts->Stamps.size() == 4 && contacts->FaceParents.size() == 4,
        "each source face retains its terrain contribution and parent");
  if (!contacts) { return; }
  for (const bool reverse : {false, true}) {
    auto stamps = contacts->Stamps;
    if (reverse) { std::ranges::reverse(stamps); }
    auto candidate = Candidate(0);
    const auto effects =
        DeformTerrain(stamps, candidate, TangentFrame::At({}), kLayout, kEarthworkLimitM);
    CHECK(effects.Structures == 0, "both contacts fit the declared local earthwork budget");
    CheckGroundMesh(candidate, kGroundTopM - SurfaceBodyBudget::kDefaultThicknessM);
  }
  const std::span<const EarthworkStamp> overhead(contacts->Stamps.data() + 2, 2);
  auto low = Candidate(0);
  (void)DeformTerrain(overhead, low, TangentFrame::At({}), kLayout, kEarthworkLimitM);
  CHECK(std::ranges::all_of(low.Sheets[0].Nodes, [](float height) { return height == 0; }),
        "an overhead span never raises low ground to its deck");
  auto high = Candidate(kHighTerrainM);
  (void)DeformTerrain(overhead, high, TangentFrame::At({}), kLayout, kEarthworkLimitM);
  CheckGroundMesh(high, kBridgeTopM - SurfaceBodyBudget::kDefaultThicknessM - kClearanceM);
}

void SharedGroundWinsOnlyOnItsActualSharedFace() {
  const auto body = Body(Sheets::Shared);
  CHECK(body.has_value(), "qualified shared faces carry both source bits");
  if (!body) { return; }
  const std::array sources{
      SurfaceGroundSource{.Effect = SurfaceGroundEffect::OverheadClearance,
                          .ClearanceM = kClearanceM},
      SurfaceGroundSource{.Effect = SurfaceGroundEffect::GroundContact, .ContactKey = 7}};
  const auto contacts = BuildSurfaceEarthworks(*body, sources, {.ApronM = 0});
  CHECK(contacts && contacts->Stamps.size() == 2 && contacts->Stamps[0].Fills &&
            contacts->Stamps[0].CorridorKey == 7,
        "a known shared ground landing remains supported instead of cutting its own foundation");
  if (!contacts) { return; }
  auto candidate = Candidate(0);
  (void)DeformTerrain(contacts->Stamps, candidate, TangentFrame::At({}), kLayout, kEarthworkLimitM);
  CheckGroundMesh(candidate, kGroundTopM - SurfaceBodyBudget::kDefaultThicknessM);
  auto reversed = sources;
  std::ranges::reverse(reversed);
  const auto same = BuildSurfaceEarthworks(*body, reversed, {.ApronM = 0});
  CHECK(same && same->Stamps == contacts->Stamps,
        "shared contact priority does not depend on source enumeration");
  reversed[1] = reversed[0];
  ++reversed[1].ContactKey;
  const auto conflict = BuildSurfaceEarthworks(*body, reversed);
  CHECK(!conflict && conflict.error() == SurfaceEarthworkError::ConflictingContactKeys,
        "a shared physical face requires one declared ground-contact group");
}

void ExplicitRolesAndBoundedInputsAreRequired() {
  const auto body = Body(Sheets::Single);
  CHECK(body.has_value(), "ordinary fixture closes");
  if (!body) { return; }
  std::array<SurfaceGroundSource, 1> sources{};
  const auto unspecified = BuildSurfaceEarthworks(*body, sources);
  CHECK(!unspecified && unspecified.error() == SurfaceEarthworkError::InvalidInput,
        "terrain effect cannot be guessed from source height or order");
  sources[0].Effect = SurfaceGroundEffect::NoEarthwork;
  const auto detached = BuildSurfaceEarthworks(*body, sources);
  CHECK(detached && detached->Stamps.empty() && detached->NoEarthworkFaces == 2,
        "an explicit no-earthwork body leaves the height field unchanged");
  const auto limited = BuildSurfaceEarthworks(*body, sources, {.Faces = 0});
  CHECK(!limited && limited.error() == SurfaceEarthworkError::CapacityExceeded,
        "input face limits apply even to detached bodies");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = BuildSurfaceEarthworks(*body, sources, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == SurfaceEarthworkError::Canceled,
        "canceled terrain conversion never publishes partial contacts");
}
}

int main() {
  GroundAndIndependentOverheadHaveDifferentEffects();
  SharedGroundWinsOnlyOnItsActualSharedFace();
  ExplicitRolesAndBoundedInputsAreRequired();
  return Report();
}

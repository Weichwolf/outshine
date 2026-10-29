#include "CanonicalGroundFieldFixture.h"
#include "GroundPortPlan.h"
#include "TransportNetworkSnapshot.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

GroundPortFieldScope DeclaredScope(LongitudeLatitude a, bool reversed) {
  auto b = a;
  b.LongitudeDeg += .0004;
  b.LatitudeDeg += .00005;
  std::vector<World::TransportNode> nodes{
      {.SourceNodeId = 1, .LatitudeDeg = a.LatitudeDeg, .LongitudeDeg = a.LongitudeDeg},
      {.SourceNodeId = 2, .LatitudeDeg = b.LatitudeDeg, .LongitudeDeg = b.LongitudeDeg}};
  World::TransportEdge edge{.Id = {.PathId = 42},
                            .FromNodeId = 1,
                            .ToNodeId = 2,
                            .Modes = static_cast<uint8_t>(World::TransportMode::Motor),
                            .WidthM = 8};
  if (reversed) {
    edge.Id.Direction = World::EdgeDirection::Reverse;
    std::swap(edge.FromNodeId, edge.ToNodeId);
  }
  const Data::SourceIdentity identity{.DatasetId = "declared-original-boundary", .Revision = "1"};
  World::TransportTopology graph(identity, std::move(nodes), {edge}, 0);
  auto snapshot = std::make_shared<const World::TransportNetworkSnapshot>(
      World::ResolvedTransport{.Graph = std::move(graph), .Routes = {}},
      std::vector<GeographicBounds>{},
      World::TransportLoadMetrics{});
  GroundPortFieldScope scope;
  scope.Source = {.Source = std::move(snapshot),
                  .Nodes = {0, 1},
                  .SourceEdges = {0},
                  .WidthM = 8,
                  .MaximumGradient = .3,
                  .Work = {}};
  scope.Info.Input = {.DatasetId = identity.DatasetId,
                      .Version = "declared-fixture/1",
                      .Revision = "1",
                      .ItemId = "whole-way/42",
                      .Content = {}};
  scope.Info.Input.Content.fill(1);
  scope.Info.ContentBinding.fill(2);
  for (uint8_t side = 0; side < 2; ++side) {
    scope.Info.Ends.push_back(
        {.Kind = TransportSourceEndKind::Natural,
         .Ground = {.Edge = edge.Id,
                    .Node = static_cast<uint64_t>(side) + 1,
                    .PointCount = 2,
                    .End = static_cast<uint8_t>(side ^ static_cast<uint8_t>(edge.Id.Direction)),
                    .Terminal = true,
                    .OwnsEnd = true},
         .Relations = {}});
  }
  return scope;
}

void CheckPlan(const GroundPortField &field,
               GeoCellId core,
               LongitudeLatitude original,
               bool reversed) {
  auto body = BuildGroundPortBodies(field, {.ThicknessM = .3, .ApronM = 2, .ReliefM = 3});
  CHECK(body.has_value(), "the whole declared source retains qualified original DEM contacts");
  if (!body) { return; }
  const std::array recipes{GroundPortPlanPathRecipe{
      .PathId = 42, .Material = "asphalt", .Use = TransportUse::Road, .SpeedMps = 13}};
  const std::array omissions{TransportOmission{
      .SourceId = "scope/pending", .Reason = TransportOmissionReason::UnsupportedRepresentation}};
  const auto product = BuildGroundPortPartialPlan(field, *body, core, recipes, omissions);
  CHECK(product.has_value(),
        "a declared original end on the exact core boundary keeps its physical profile");
  if (!product) { return; }
  const auto &plan = product->Plan;
  CHECK(plan.SourceEnds.size() == 1 && !plan.Paths.empty(),
        "the selected core owns the one actual original end");
  if (plan.SourceEnds.size() != 1 || plan.Paths.empty()) { return; }
  const auto &end = plan.SourceEnds.front();
  CHECK(end.Topology.Kind == TransportSourceEndKind::Natural && end.Topology.Ground.Node == 1 &&
            end.Anchor.LongitudeDeg == original.LongitudeDeg &&
            end.Anchor.LatitudeDeg == original.LatitudeDeg,
        "explicit original ownership survives at the coincident geographic boundary");
  const auto *reference = std::get_if<TransportSourceEndReference>(
      &plan.Paths.front().Ends[static_cast<size_t>(reversed)]);
  CHECK(reference && reference->End == 0,
        "the source endpoint is not duplicated as a geometric clipping contact");
  CHECK(std::ranges::all_of(plan.Ports,
                            [&](const auto &port) {
                              return port.Contact.Gate.EastM == 0 &&
                                     port.Contact.Gate.NorthM == 0 &&
                                     (port.Anchor.LongitudeDeg != original.LongitudeDeg ||
                                      port.Anchor.LatitudeDeg != original.LatitudeDeg);
                            }),
        "all remaining boundary ports are actual canonical core cuts");
  auto inside = original;
  inside.LongitudeDeg += .0004;
  inside.LatitudeDeg += .00005;
  const auto direction = TangentFrame::At(end.Anchor).ToLocalGroundPosition(inside);
  CHECK(end.Contact.Gate.OutE * direction.EastM + end.Contact.Gate.OutN * direction.NorthM < 0,
        "the physical start profile faces outward for either directed representation");
  const TransportPlanCodecLimits limits{.PackageBytesMost = 1 << 20,
                                        .DecodedCapacityMost = 1 << 22};
  const auto packet = EncodeTransportPlan(plan, limits);
  CHECK(packet.has_value(), "the qualified original profile and real core cuts persist together");
  if (packet) {
    const auto decoded = DecodeTransportPlan(*packet, limits);
    CHECK(decoded && decoded->SourceEnds.size() == 1 &&
              decoded->SourceEnds.front().Input.Content == end.Input.Content,
          "restoration retains the explicit original profile without constructing another boundary "
          "port");
  }
}
}

int main() {
  auto fields = outshine::Test::CanonicalGroundFixture::LoadFields();
  CHECK(fields.size() == 4,
        "the unchanged captured fields supply the actual DEM and a covered location");
  if (fields.size() != 4) { return outshine::Test::Report(); }
  constexpr unsigned level = 21;
  constexpr double count = uint32_t{1} << level;
  auto a = fields[2].Domain().Anchor;
  const auto x = static_cast<uint32_t>((a.LongitudeDeg + 180) / 360 * count);
  const auto y = static_cast<uint32_t>((a.LatitudeDeg + 90) / 180 * count);
  a.LongitudeDeg = static_cast<double>(x) / count * 360 - 180;
  for (const bool reversed : std::array{false, true}) {
    auto request =
        outshine::Generators::GroundPortFieldRequest::PrepareScoped(DeclaredScope(a, reversed));
    CHECK(request.has_value(), "the explicitly declared whole source has a supported field recipe");
    if (!request) { continue; }
    auto terrain =
        std::make_shared<const outshine::Generators::GroundPortTerrain>(fields[2].Terrain());
    auto field =
        outshine::Generators::GroundPortField::Make(std::move(*request), std::move(terrain), 3);
    CHECK(field.has_value(), "the unmodified actual DEM qualifies the finite field");
    if (field) { CheckPlan(*field, {.Level = level, .X = x, .Y = y}, a, reversed); }
  }
  return outshine::Test::Report();
}

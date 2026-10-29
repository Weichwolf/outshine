#include "TransportPlanFixture.h"
#include "Check.h"
#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <string_view>
#include <tuple>
#include <variant>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

bool Bind(TransportPlan &plan) {
  auto &end = plan.SourceEnds.front();
  const auto &member = end.Topology.Ground;
  const auto side = static_cast<uint8_t>(member.End ^ static_cast<uint8_t>(member.Edge.Direction));
  end.Input.ItemId =
      std::format("native/{}/{}/{}", member.Edge.PathId, member.Edge.SegmentOrdinal, side);
  end.Input.Content = TransportSourceEndContentBinding(end);
  std::erase_if(plan.Inputs, [](const auto &input) {
    return input.Version == "ground-source-end/1" || input.DatasetId == "outshine/ground-port-path";
  });
  plan.Inputs.push_back(end.Input);
  plan.Inputs.push_back({.DatasetId = "outshine/ground-port-path",
                         .Version = "1",
                         .Revision = "1",
                         .ItemId = end.SourceId,
                         .Content = end.Inputs});
  std::ranges::sort(plan.Inputs, {}, [](const TransportInputReference &input) {
    return std::tuple{std::string_view(input.DatasetId),
                      std::string_view(input.Version),
                      std::string_view(input.Revision),
                      std::string_view(input.ItemId)};
  });
  const auto binding =
      TransportPlanInputBinding(plan.Inputs, kTransportFixtureLimits.PackageBytesMost);
  if (!binding) { return false; }
  plan.InputBinding = *binding;
  return true;
}

TransportPlan NaturalPlan(TransportPlan plan) {
  auto &path = plan.Paths.front();
  path.SourceId = "native/42/0";
  plan.Ports.front().SourceId = path.SourceId;
  plan.Junctions.front().Sources.push_back(path.SourceId);
  std::ranges::sort(plan.Junctions.front().Sources);
  TransportSourceEnd end;
  end.Input = {.DatasetId = "fixture-source",
               .Version = "ground-source-end/1",
               .Revision = "snapshot-1",
               .ItemId = {},
               .Content = {}};
  end.SourceId = path.SourceId;
  end.ScopeBinding.fill(1);
  end.Inputs = plan.Ports.front().Inputs;
  end.Topology = {.Kind = TransportSourceEndKind::Natural,
                  .Ground = {.Edge = {.PathId = 42},
                             .Node = 8,
                             .PointCount = 2,
                             .End = 1,
                             .Terminal = true,
                             .OwnsEnd = true},
                  .Relations = {}};
  end.Anchor = plan.Ports.front().Anchor;
  end.Contact = plan.Ports.front().Contact;
  end.Profile = path.Profile;
  end.CrossfallRad = plan.Ports.front().CrossfallRad;
  plan.SourceEnds.push_back(end);
  path.Ends[1] = TransportSourceEndReference{.End = 0};
  plan.Stations[path.FirstStation + path.StationCount - 1].Node = 8;
  return plan;
}

void CheckNatural(TransportPlan fixture) {
  auto plan = NaturalPlan(std::move(fixture));
  const auto end = plan.SourceEnds.front();
  CHECK(Bind(plan) && ValidateTransportPlan(plan),
        "an explicitly owned original natural end is a valid complete product");
  const auto encoded = EncodeTransportPlan(plan, kTransportFixtureLimits);
  CHECK(encoded.has_value(), "the complete natural profile and original member serialize");
  if (encoded) {
    const auto restored = DecodeTransportPlan(*encoded, kTransportFixtureLimits);
    CHECK(restored && restored->SourceEnds.size() == 1 &&
              restored->SourceEnds.front().Topology == end.Topology &&
              EncodeTransportPlan(*restored, kTransportFixtureLimits) == encoded,
          "all explicit natural-end evidence survives the owned codec round trip");
  }
  auto alias = plan;
  alias.SourceEnds.front().Topology.Ground.Edge.Direction = World::EdgeDirection::Reverse;
  alias.SourceEnds.front().Topology.Ground.End = 0;
  CHECK(Bind(alias) && ValidateTransportPlan(alias) && alias.InputBinding == plan.InputBinding,
        "the opposite directed alias preserves the same physical end and input binding");
  auto missing = plan;
  missing.SourceEnds.front().Topology.Ground.OwnsEnd = false;
  CHECK(Bind(missing) && !ValidateTransportPlan(missing),
        "rehashing cannot qualify an unowned natural end");
  missing = plan;
  missing.SourceEnds.front().Topology.Ground.Terminal = false;
  CHECK(Bind(missing) && !ValidateTransportPlan(missing),
        "a degree-one claim cannot replace explicit source-terminal evidence");
  missing = plan;
  missing.SourceEnds.front().Topology.Ground.SourceStep = 1;
  CHECK(Bind(missing) && !ValidateTransportPlan(missing),
        "the original step must belong to the declared whole way");
  missing = plan;
  missing.SourceEnds.front().Topology.Ground.End = 0;
  CHECK(Bind(missing) && !ValidateTransportPlan(missing),
        "the physical side must agree with the owned native node");
  missing = plan;
  missing.SourceEnds.front().Topology.Kind = TransportSourceEndKind::Unresolved;
  CHECK(Bind(missing) && !ValidateTransportPlan(missing),
        "unresolved incidence cannot become complete coverage");
  missing.Coverage = TransportCoverage::Partial;
  missing.Omissions.push_back(
      {.SourceId = end.SourceId, .Reason = TransportOmissionReason::MissingBoundary});
  CHECK(Bind(missing) && ValidateTransportPlan(missing),
        "the same unresolved end remains explicit in a partial product");
  missing = plan;
  missing.SourceEnds.front().Input.DatasetId = "foreign-source";
  CHECK(Bind(missing) && !ValidateTransportPlan(missing),
        "a foreign source identity cannot label an owned native member");
  missing = plan;
  std::get<TransportSourceEndReference>(missing.Paths.front().Ends[1]).End =
      std::numeric_limits<uint32_t>::max();
  CHECK(!ValidateTransportPlan(missing),
        "an endpoint reference cannot exceed its owned source table");
}

void CheckSourceNodes(TransportPlan fixture) {
  auto plan = NaturalPlan(std::move(fixture));
  auto &topology = plan.SourceEnds.front().Topology;
  topology.Kind = TransportSourceEndKind::SourceNode;
  topology.Relations = {{.Edge = {.PathId = 42},
                         .Node = 7,
                         .PointCount = 2,
                         .End = 0,
                         .Terminal = true,
                         .OwnsEnd = true}};
  CHECK(Bind(plan), "the opposite-end counterexample has a fully rebuilt input binding");
  const auto wrong = ValidateTransportPlan(plan);
  CHECK(!wrong && wrong.error() == TransportPlanCodecError::InvalidSourceEnd,
        "different actual native nodes cannot declare a SourceNode connection after rehashing");
  topology.Ground.Node = 7;
  topology.Ground.End = 0;
  topology.Relations = {{.Edge = {.PathId = 42, .SegmentOrdinal = 1},
                         .Node = 7,
                         .SourceStep = 1,
                         .PointCount = 3,
                         .End = 1,
                         .Terminal = true,
                         .OwnsEnd = true}};
  auto &end = plan.SourceEnds.front();
  end.Anchor = plan.Anchor;
  end.Contact.Gate = plan.Junctions.front().Gates.front();
  end.Contact.Plane = plan.Junctions.front().Plane;
  auto &path = plan.Paths.front();
  path.Ends = {TransportSourceEndReference{.End = 0}, TransportBoundaryPortReference{.Port = 0}};
  plan.Stations[path.FirstStation + path.StationCount - 1].Node = 0;
  CHECK(Bind(plan) && ValidateTransportPlan(plan),
        "different original segments incident to the same native node retain a valid connection");
  const auto encoded = EncodeTransportPlan(plan, kTransportFixtureLimits);
  CHECK(encoded && DecodeTransportPlan(*encoded, kTransportFixtureLimits),
        "the qualified native-node connection survives the owned codec boundary");
  auto alias = plan;
  auto &relation = alias.SourceEnds.front().Topology.Relations.front();
  relation.Edge.Direction = World::EdgeDirection::Reverse;
  relation.End = 0;
  CHECK(Bind(alias) && ValidateTransportPlan(alias) && alias.InputBinding == plan.InputBinding,
        "the opposite directed alias preserves the same physical native-node incidence");
}
}

int main() {
  const auto fixture = outshine::Test::TransportPlanFixture();
  CHECK(fixture.has_value(), "the declared native fixture is available");
  if (fixture) {
    CheckNatural(*fixture);
    CheckSourceNodes(*fixture);
  }
  return outshine::Test::Report();
}

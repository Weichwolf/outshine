#include "CanonicalGroundFieldFixture.h"
#include "CppAllocationProbe.h"
#include "Check.h"
#include "GroundPortCore.h"
#include "GroundPortPlan.h"
#include "ReadTextFile.h"
#include "TransportNetworkSnapshot.h"
#include "TransportPlanNetworkBinding.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <ios>
#include <string_view>
#include <map>
#include <memory>
#include <limits>
#include <numeric>
#include <print>
#include <ratio>
#include <set>
#include <optional>
#include <stop_token>
#include <variant>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr TransportPlanCodecLimits kCodec{.PackageBytesMost = 16777216,
                                          .DecodedCapacityMost = 67108864};

std::vector<GroundPortPlanPathRecipe> Recipes(const GroundPortField &field) {
  std::map<uint64_t, TransportUse> paths;
  for (const auto at : field.Source().SourceEdges) {
    const auto &edge = field.Source().Source->Topology().Edges()[at];
    auto use = TransportUse::Path;
    if ((edge.Modes & static_cast<uint8_t>(World::TransportMode::Motor)) != 0) {
      use = TransportUse::Road;
    } else if ((edge.Modes & static_cast<uint8_t>(World::TransportMode::Rail)) != 0) {
      use = TransportUse::Rail;
    }
    paths.emplace(edge.Id.PathId, use);
  }
  std::vector<GroundPortPlanPathRecipe> recipes;
  recipes.reserve(paths.size());
  for (const auto &[path, use] : paths) {
    recipes.push_back({.PathId = path, .Material = "functional-ground", .Use = use, .SpeedMps = 8});
  }
  return recipes;
}

std::vector<GeoCellId> Cores(const GroundPortField &field) {
  constexpr unsigned level = 21;
  constexpr double count = uint32_t{1} << level;
  std::set<std::pair<uint32_t, uint32_t>> cells;
  const auto &graph = field.Source().Source->Topology();
  for (const auto index : field.Source().SourceEdges) {
    const auto &edge = graph.Edges()[index];
    const auto *a = graph.FindNode(edge.FromNodeId);
    const auto *b = graph.FindNode(edge.ToNodeId);
    CHECK(a != nullptr && b != nullptr,
          "actual source nodes remain available to the calling test driver");
    if (a == nullptr || b == nullptr) { continue; }
    const auto x = static_cast<uint32_t>((std::midpoint(a->LongitudeDeg, b->LongitudeDeg) + 180) /
                                         360 * count);
    const auto y =
        static_cast<uint32_t>((std::midpoint(a->LatitudeDeg, b->LatitudeDeg) + 90) / 180 * count);
    cells.emplace(x, y);
    cells.emplace(x - 1, y);
    cells.emplace(x + 1, y);
    cells.emplace(x, y - 1);
    cells.emplace(x, y + 1);
    for (const auto *endpoint : {a, b}) {
      const auto ex = static_cast<uint32_t>((endpoint->LongitudeDeg + 180) / 360 * count);
      const auto ey = static_cast<uint32_t>((endpoint->LatitudeDeg + 90) / 180 * count);
      cells.emplace(ex, ey);
      cells.emplace(ex - 1, ey);
      cells.emplace(ex + 1, ey);
      cells.emplace(ex, ey - 1);
      cells.emplace(ex, ey + 1);
    }
  }
  std::vector<GeoCellId> result;
  result.reserve(cells.size());
  for (const auto &[x, y] : cells) { result.push_back({.Level = level, .X = x, .Y = y}); }
  return result;
}

void CheckBodies(const GroundPortPlanProduct &product, const GroundPortBodies &expected) {
  CHECK(product.Plan.Bodies.size() == expected.Bodies.size(),
        "all actual interval bodies reach the plan");
  size_t contact = 0;
  for (size_t at = 0; at < expected.Bodies.size(); ++at) {
    const auto &a = product.Plan.Bodies[at];
    const auto &b = expected.Bodies[at].Geometry;
    CHECK(
        a.Points == b.Points && a.Faces == b.Faces && a.WallKinds == b.WallKinds &&
            a.WallParents == b.WallParents && a.SurfaceFaces == b.SurfaceFaces,
        "the explicit plan body is the actual checked native core geometry, including interfaces");
    for (const auto &stamp : expected.Bodies[at].TerrainContacts) {
      CHECK(contact < product.Plan.TerrainContacts.size(), "original owned contacts are not lost");
      if (contact >= product.Plan.TerrainContacts.size()) { continue; }
      const auto &actual = product.Plan.TerrainContacts[contact++];
      CHECK(actual == stamp, "every owned contact field remains exactly equal after transfer");
    }
  }
  const auto bounds = GroundPortBodyBoundsEcef(expected);
  CHECK(contact == product.Plan.TerrainContacts.size() && product.BoundsEcef.Min == bounds.Min &&
            product.BoundsEcef.Max == bounds.Max,
        "the plan retains every owned contact and the actual full-geometry/contact ECEF bounds");
}

void CheckSourceRecords(const GroundPortField &field, const TransportPlan &plan) {
  if (!field.Scope()) {
    CHECK(plan.SourceEnds.empty(),
          "a strict source snapshot without original incidence cannot invent natural endpoints");
    return;
  }
  for (const auto &end : plan.SourceEnds) {
    CHECK(
        end.ScopeBinding == field.Scope()->ContentBinding &&
            end.Input.Content == TransportSourceEndContentBinding(end),
        "portable endpoint identity binds the qualified original scope and full physical profile");
    const auto key = TransportSourceMemberKey(end.Topology.Ground);
    const auto original = std::ranges::find_if(field.Scope()->Ends, [&](const auto &candidate) {
      return TransportSourceMemberKey(candidate.Ground) == key;
    });
    CHECK(original != field.Scope()->Ends.end() && *original == end.Topology,
          "all original directed ends, nodes, terminal ownership and relations remain owned");
    for (const auto &relation : end.Topology.Relations) {
      CHECK(field.Source().Source->Topology().FindEdge(relation.Edge) != nullptr,
            "the retained original relation names an actual source edge");
      if (end.Topology.Kind == TransportSourceEndKind::External) {
        CHECK(plan.Network->Topology().FindEdge(relation.Edge) == nullptr,
              "an unqualified original bridge/layer arm is not a functional plan edge");
      }
    }
    auto reversed = end;
    reversed.Topology.Ground.Edge.Direction =
        reversed.Topology.Ground.Edge.Direction == World::EdgeDirection::Forward
            ? World::EdgeDirection::Reverse
            : World::EdgeDirection::Forward;
    reversed.Topology.Ground.End ^= 1U;
    CHECK(TransportSourceEndContentBinding(reversed) == end.Input.Content,
          "a directed alias with the same physical endpoint preserves endpoint identity");
    reversed.Topology.Ground.End ^= 1U;
    CHECK(TransportSourceEndContentBinding(reversed) != end.Input.Content,
          "changing only the physical endpoint is a different original relation");
  }
}

void CheckSourceReferences(const TransportPlan &plan) {
  for (const auto &path : plan.Paths) {
    for (size_t side = 0; side < 2; ++side) {
      const auto *reference = std::get_if<TransportSourceEndReference>(&path.Ends[side]);
      if (reference == nullptr) { continue; }
      CHECK(reference->End < plan.SourceEnds.size(),
            "path endpoints reference their own plan table");
      if (reference->End >= plan.SourceEnds.size()) { continue; }
      const auto &end = plan.SourceEnds[reference->End];
      const auto &station =
          plan.Stations[path.FirstStation + (side == 0 ? 0 : path.StationCount - 1)];
      CHECK(end.SourceId == path.SourceId && end.Topology.Ground.Node == station.Node,
            "the physical endpoint and actual path station retain their original identity");
    }
  }
}

void CheckRejectedSourceEnds(const TransportPlan &plan) {
  if (!plan.SourceEnds.empty()) {
    auto corrupt = plan;
    corrupt.SourceEnds.front().Topology.Ground.End ^= 1U;
    CHECK(!ValidateTransportPlan(corrupt), "an end-side mutation invalidates its original binding");
    corrupt = plan;
    corrupt.SourceEnds.front().Contact.Gate.GradeM += 1;
    CHECK(!ValidateTransportPlan(corrupt), "a profile mutation invalidates its endpoint binding");
    corrupt = plan;
    corrupt.SourceEnds.front().ScopeBinding.front() ^= 1U;
    CHECK(!ValidateTransportPlan(corrupt), "another source scope cannot reinterpret an endpoint");
    corrupt = plan;
    for (auto &path : corrupt.Paths) {
      for (auto &end : path.Ends) {
        if (auto *reference = std::get_if<TransportSourceEndReference>(&end)) {
          reference->End = std::numeric_limits<uint32_t>::max();
        }
      }
    }
    CHECK(!ValidateTransportPlan(corrupt), "dangling plan-local endpoint references are rejected");
  }
}

void CheckSourceEnds(const GroundPortField &field, const TransportPlan &plan) {
  CheckSourceRecords(field, plan);
  CheckSourceReferences(plan);
  CheckRejectedSourceEnds(plan);
}

struct PortEvidence {
  std::array<uint8_t, 32> Inputs{};
  std::array<uint64_t, 13> Words{};
  bool operator==(const PortEvidence &) const = default;
};

using PortKey =
    std::tuple<int, TransportBoundaryAxis, uint32_t, uint32_t, std::string, double, double>;
using Ports = std::map<PortKey, PortEvidence>;

size_t CheckPorts(const TransportPlan &plan, Ports &ports) {
  size_t shared = 0;
  for (const auto &port : plan.Ports) {
    const auto &gate = port.Contact.Gate;
    const std::array words{port.Anchor.LongitudeDeg,
                           port.Anchor.LatitudeDeg,
                           gate.EastM,
                           gate.NorthM,
                           gate.GradeM,
                           gate.OutE,
                           gate.OutN,
                           gate.HalfWidthM,
                           gate.ShoulderM,
                           gate.ThicknessM,
                           port.Contact.Plane.SlopeE,
                           port.Contact.Plane.SlopeN,
                           port.CrossfallRad};
    const PortEvidence value{.Inputs = port.Inputs,
                             .Words = std::bit_cast<std::array<uint64_t, 13>>(words)};
    const PortKey key{port.Boundary.Level,
                      port.Boundary.Axis,
                      port.Boundary.Line,
                      port.Boundary.Segment,
                      port.SourceId,
                      port.Anchor.LongitudeDeg,
                      port.Anchor.LatitudeDeg};
    const auto [old, inserted] = ports.emplace(key, value);
    if (!inserted) {
      ++shared;
      CHECK(old->second == value,
            "neighboring native plans share bit-exact own-ENU port words and input binding");
    }
  }
  return shared;
}

void CheckRecipeBinding(const GroundPortField &field,
                        const GroundPortBodies &whole,
                        GeoCellId core,
                        const std::vector<GroundPortPlanPathRecipe> &recipes,
                        std::span<const TransportOmission> omissions,
                        const GroundPortPlanProduct &original) {
  auto changed = recipes;
  for (auto &recipe : changed) { recipe.SpeedMps += 1; }
  const auto altered = BuildGroundPortPartialPlan(field, whole, core, changed, omissions);
  CHECK(altered && altered->Plan.InputBinding != original.Plan.InputBinding,
        "consumed functional recipe changes invalidate the plan binding");
  if (altered) {
    CHECK(altered->Plan.Bodies[0].Points == original.Plan.Bodies[0].Points,
          "a speed recipe changes functional metadata without rebuilding a different height field");
  }
  auto unused = recipes;
  unused.push_back(
      {.PathId = std::numeric_limits<uint64_t>::max(), .Material = "unused", .SpeedMps = 123});
  const auto unrelated = BuildGroundPortPartialPlan(field, whole, core, unused, omissions);
  CHECK(unrelated && unrelated->Plan.InputBinding == original.Plan.InputBinding,
        "an unconsumed source recipe never enters the plan input binding");
  auto alternate = GroundPortPlanBudget{};
  alternate.OutputCapacityBytes *= 2;
  const auto admitted =
      BuildGroundPortPartialPlan(field, whole, core, recipes, omissions, alternate);
  CHECK(admitted && admitted->Plan.InputBinding == original.Plan.InputBinding,
        "admission capacity does not become functional content");
  auto full = original.Plan;
  full.Coverage = TransportCoverage::Complete;
  full.Omissions.clear();
  CHECK(!ValidateTransportPlan(full),
        "real core bodies do not prove full geographic source closure");
  std::stop_source stopped;
  stopped.request_stop();
  CHECK(
      !BuildGroundPortPartialPlan(field, whole, core, recipes, omissions, {}, stopped.get_token()),
      "canceled exports never publish an apparently complete product");
  CHECK(!BuildGroundPortPartialPlan(field, whole, core, recipes, {}),
        "a caller must explicitly declare the partial query omissions");
  CHECK(!BuildGroundPortPartialPlan(field, whole, core, {}, omissions),
        "missing consumed functional recipes are not fabricated");
  GroundPortPlanBudget cap;
  cap.Paths = 0;
  CHECK(!BuildGroundPortPartialPlan(field, whole, core, recipes, omissions, cap),
        "a work cap refuses instead of publishing a shortened plan");
}

struct FieldFixture {
  const GroundPortField &Field;
  const GroundPortBodies &Whole;
  const std::vector<GroundPortPlanPathRecipe> &Recipes;
  std::span<const TransportOmission> Omissions;
};

struct FieldEvidence {
  size_t Which = 0;
  size_t Plans = 0;
  size_t Shared = 0;
  Ports SeenPorts;
  bool RecipeChecked = false;
};

std::optional<std::vector<uint8_t>>
CheckCore(const FieldFixture &fixture, GeoCellId core, FieldEvidence &evidence) {
  const auto &field = fixture.Field;
  const auto &whole = fixture.Whole;
  const auto &recipes = fixture.Recipes;
  const auto omissions = fixture.Omissions;
  const auto &graph = field.Source().Source->Topology();
  const auto selection = SelectGroundPortCore(field,
                                              core,
                                              {.SourceNodesMost = graph.Nodes().size(),
                                               .SourceEdgesMost = graph.Edges().size(),
                                               .SegmentsMost = 2048,
                                               .ContainerCapacityMost = 1048576});
  CHECK(selection.has_value(), "the calling test determines finite core demand before work");
  if (!selection || selection->Segments.empty()) { return std::nullopt; }
  const auto start = std::chrono::steady_clock::now();
  BeginCppAllocationProbe();
  auto product = BuildGroundPortPartialPlan(field, whole, core, recipes, omissions);
  const auto allocations = EndCppAllocationProbe();
  const auto duration = std::chrono::steady_clock::now() - start;
  CHECK(product.has_value(), "the actual field exports its functional core plan");
  if (!product) {
    std::visit(
        [index = product.error().index()](auto error) {
          std::println("GROUND_PLAN_ERROR kind={} value={}", index, static_cast<unsigned>(error));
        },
        product.error());
    return std::nullopt;
  }
  ++evidence.Plans;
  CHECK(product->Plan.Coverage == TransportCoverage::Partial &&
            product->Plan.Paths.size() == product->Plan.Bodies.size() && product->Plan.Network &&
            product->Plan.Network->Coverage().empty(),
        "real geometry stays explicitly partial until whole geographic closure is qualified");
  auto expected = SliceGroundPortBodies(field, whole, *selection);
  CHECK(expected.has_value(), "the existing qualified core geometry remains the comparison");
  if (expected) {
    auto frame = ReframeGroundPortBodies(std::move(*expected), product->Plan.Anchor);
    CHECK(frame.has_value(), "the original core enters the same exact geographic frame");
    if (frame) { CheckBodies(*product, *frame); }
  }
  evidence.Shared += CheckPorts(product->Plan, evidence.SeenPorts);
  CheckSourceEnds(field, product->Plan);
  auto encoded = EncodeTransportPlan(product->Plan, kCodec);
  CHECK(encoded.has_value(), "the real functional product crosses the existing native plan codec");
  if (!evidence.RecipeChecked) {
    CheckRecipeBinding(field, whole, core, recipes, omissions, *product);
    evidence.RecipeChecked = true;
  }
  std::println("GROUND_PLAN field={} core={}/{}/{} paths={} ports={} contacts={} capacity_bytes={} "
               "allocations={} requested_bytes={} export_us={:.3f}",
               evidence.Which,
               core.Level,
               core.X,
               core.Y,
               product->Plan.Paths.size(),
               product->Plan.Ports.size(),
               product->Plan.TerrainContacts.size(),
               product->Work.OutputCapacityBytes,
               allocations.Calls,
               allocations.Bytes,
               std::chrono::duration<double, std::micro>(duration).count());
  if (!encoded) { return std::nullopt; }
  const auto restored = DecodeTransportPlan(*encoded, kCodec);
  CHECK(restored && restored->Paths.size() == product->Plan.Paths.size() &&
            restored->Bodies.size() == product->Plan.Bodies.size() &&
            restored->TerrainContacts.size() == product->Plan.TerrainContacts.size(),
        "functional paths, complete bodies and original terrain contacts survive "
        "serialization");

  if (restored) {
    const auto again = EncodeTransportPlan(*restored, kCodec);
    CHECK(again && *again == *encoded,
          "every encoded functional field survives the actual codec round trip exactly");
  }
  return std::move(*encoded);
}

void ObservePacket(size_t which,
                   GeoCellId core,
                   const std::vector<uint8_t> &encoded,
                   std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> &firstPublication,
                   std::vector<uint8_t> &retained,
                   size_t &sourceEnds) {
  const auto decoded = DecodeTransportPlan(encoded, kCodec);
  CHECK(decoded.has_value(), "actual endpoint cores remain decodable");
  if (decoded) {
    sourceEnds += decoded->SourceEnds.size();
    if (retained.empty() && std::ranges::any_of(decoded->SourceEnds, [](const auto &end) {
          return end.Topology.Kind == TransportSourceEndKind::External;
        })) {
      retained = encoded;
    }
  }
  if (which == 2) { firstPublication.emplace(std::pair{core.X, core.Y}, encoded); }
  if (which == 3) {
    const auto old = firstPublication.find({core.X, core.Y});
    CHECK(old != firstPublication.end() && old->second == encoded,
          "actual repeated source publication and reverse core order preserve the full plan bytes");
  }
}

int DecodePacket(const char *path) {
  const auto bytes = ReadTextFile(path, kCodec.PackageBytesMost);
  CHECK(bytes.has_value(), "a fresh codec process reads only its persisted native packet");
  if (!bytes) { return Report(); }
  const auto plan = DecodeTransportPlan(
      std::span(reinterpret_cast<const uint8_t *>(bytes->data()), bytes->size()), kCodec);
  CHECK(plan && !plan->SourceEnds.empty() && plan->Coverage == TransportCoverage::Partial,
        "source-owned endpoint profiles restore without opening original sources");
  if (plan) {
    for (const auto &end : plan->SourceEnds) {
      CHECK(end.Input.Content == TransportSourceEndContentBinding(end),
            "the fresh decoder retains complete endpoint content binding");
    }
  }
  return Report();
}

}

int main(int argc, char **argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--decode") { return DecodePacket(argv[2]); }
  auto fields = CanonicalGroundFixture::LoadFields();
  CHECK(fields.size() == 4, "original affine/C1 fields and both actual MVT publications load");
  if (fields.size() != 4) { return Report(); }
  std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> firstPublication;
  size_t plans = 0;
  size_t shared = 0;
  size_t sourceEnds = 0;
  std::vector<uint8_t> retained;
  for (size_t which = 0; which < fields.size(); ++which) {
    const auto &field = fields[which];
    const auto whole = BuildGroundPortBodies(field, {.ThicknessM = .3, .ApronM = 2, .ReliefM = 3});
    CHECK(whole.has_value(),
          "the existing qualified whole body is built once for all core requests");
    if (!whole) { continue; }
    const auto recipes = Recipes(field);
    const auto &identity = field.Source().Source->SourceIdentity();
    const std::array omissions{TransportOmission{
        .SourceId = std::format("native-source/{}/{}", identity.DatasetId, identity.Revision),
        .Reason = TransportOmissionReason::UnsupportedRepresentation}};
    FieldEvidence evidence{.Which = which, .SeenPorts = {}};
    const FieldFixture fixture{
        .Field = field, .Whole = *whole, .Recipes = recipes, .Omissions = omissions};
    auto cores = Cores(field);
    if ((which & 1U) != 0) { std::ranges::reverse(cores); }
    for (const auto core : cores) {
      const auto encoded = CheckCore(fixture, core, evidence);
      if (encoded) { ObservePacket(which, core, *encoded, firstPublication, retained, sourceEnds); }
    }
    plans += evidence.Plans;
    shared += evidence.Shared;
  }
  CHECK(plans >= 8 && shared >= 4,
        "real adjacent cores exercise shared ports across affine/C1/scoped fields");
  CHECK(sourceEnds > 0 && !retained.empty(),
        "actual source endpoints and external relations are exercised, not just interior cuts");
  const std::weak_ptr<const World::TransportNetworkSnapshot> released = fields[2].Source().Source;
  fields.clear();
  CHECK(released.expired(), "all original source leases expire before the retained plan restores");
  CHECK(DecodeTransportPlan(retained, kCodec).has_value(),
        "the persisted complete endpoint profile survives original source release");
  if (argc == 3 && std::string_view(argv[1]) == "--save") {
    std::ofstream output(argv[2], std::ios::binary);
    output.write(reinterpret_cast<const char *>(retained.data()),
                 static_cast<std::streamsize>(retained.size()));
    CHECK(output.good(), "the actual owned plan is saved for an independent codec process");
  }
  std::println("GROUND_PLAN source_end_records={}", sourceEnds);
  std::println("GROUND_PLAN plans={} repeated_shared_ports={}", plans, shared);
  return Report();
}

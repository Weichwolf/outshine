#include "CanonicalGroundFieldFixture.h"
#include "CppAllocationProbe.h"
#include "Check.h"
#include "GroundPortPlanNetwork.h"
#include "TransportPlanCodec.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckOriginalRows(const GroundPortField &field, const TransportPlanNetwork &projection) {
  const auto &source = *field.Source().Source;
  const auto &network = *projection.Network;
  CHECK(World::ValidateTransportNetwork(network).has_value() && network.Coverage().empty(),
        "the actual finite-field network is valid without claiming whole geographic coverage");
  for (const auto &edge : network.Topology().Edges()) {
    const auto *original = source.Topology().FindEdge(edge.Id);
    CHECK(original && edge.FromNodeId == original->FromNodeId &&
              edge.ToNodeId == original->ToNodeId && edge.Modes == original->Modes &&
              edge.Access == original->Access && edge.Facility == original->Facility &&
              edge.Surface == original->Surface && edge.WidthM == original->WidthM &&
              edge.LaneCount == original->LaneCount && edge.Layer == original->Layer &&
              edge.Bridge == original->Bridge && edge.Tunnel == original->Tunnel,
          "every exported native edge keeps its actual functional attributes and direction");
  }
  for (const auto index : field.Source().SourceEdges) {
    CHECK(network.Topology().FindEdge(source.Topology().Edges()[index].Id) != nullptr,
          "the complete qualified source chain remains available");
  }
  if (field.Scope()) {
    for (const auto &exit : field.Scope()->Exits) {
      CHECK(network.Topology().FindEdge(exit.External) != nullptr,
            "the original typed bridge/layer exits remain explicit native connections");
    }
  }
  for (const auto &path : network.Provenance().Paths) {
    const auto original = std::ranges::lower_bound(
        source.Provenance().Paths, path.PathId, {}, &World::TransportPathSource::PathId);
    CHECK(original != source.Provenance().Paths.end() && original->PathId == path.PathId &&
              original->ReferenceCount == path.ReferenceCount &&
              original->SuppliedFields == path.SuppliedFields &&
              original->DerivedFields == path.DerivedFields,
          "selected native paths retain their whole original provenance contract");
    if (original == source.Provenance().Paths.end()) { continue; }
    for (size_t at = 0; at < path.ReferenceCount; ++at) {
      const auto &reference = network.Provenance().References[path.FirstReference + at];
      const auto &before = source.Provenance().References[original->FirstReference + at];
      const auto &item = network.Provenance().Sources[reference.Source];
      const auto &oldItem = source.Provenance().Sources[before.Source];
      CHECK(
          reference.SegmentOrdinal == before.SegmentOrdinal &&
              reference.ProviderElementId == before.ProviderElementId &&
              reference.Fragment == before.Fragment && item.Identity == oldItem.Identity &&
              item.Item == oldItem.Item && item.Content == oldItem.Content,
          "the real MVT provider, fragment, source item and content remain exact after remapping");
    }
  }
}

std::vector<uint8_t> EncodeNetworkControl(const TransportPlanNetwork &projection) {
  const auto &network = *projection.Network;
  const auto &node = network.Topology().Nodes().front();
  constexpr unsigned level = 21;
  constexpr double cells = uint32_t{1} << level;
  const GeoCellId core{.Level = level,
                       .X = static_cast<uint32_t>((node.LongitudeDeg + 180) / 360 * cells),
                       .Y = static_cast<uint32_t>((node.LatitudeDeg + 90) / 180 * cells)};
  TransportPlan plan({.Core = core});
  const auto bounds = core.Bounds();
  CHECK(bounds.has_value(), "the projection-only codec control uses an actual source location");
  if (!bounds) { return {}; }
  plan.Anchor = {.LongitudeDeg = std::midpoint(bounds->WestDeg, bounds->EastDeg),
                 .LatitudeDeg = std::midpoint(bounds->SouthDeg, bounds->NorthDeg)};
  plan.Network = projection.Network;
  plan.Coverage = TransportCoverage::Partial;
  for (const auto &path : network.Provenance().Paths) {
    plan.Omissions.push_back({.SourceId = std::format("native/path/{}", path.PathId),
                              .Reason = TransportOmissionReason::ResourceDeferred});
  }
  std::ranges::sort(plan.Omissions, {}, &TransportOmission::SourceId);
  for (const auto &item : network.Provenance().Sources) {
    plan.Inputs.push_back({.DatasetId = item.Identity.DatasetId,
                           .Version = "mvt",
                           .Revision = item.Identity.Revision,
                           .ItemId = item.Item,
                           .Content = item.Content});
  }
  constexpr TransportPlanCodecLimits limits{.PackageBytesMost = 1048576,
                                            .DecodedCapacityMost = 4194304};
  const auto binding = TransportPlanInputBinding(plan.Inputs, limits.PackageBytesMost);
  CHECK(binding.has_value(), "the actual consumed MVT source references bind the codec control");
  if (!binding) { return {}; }
  plan.InputBinding = *binding;
  auto encoded = EncodeTransportPlan(plan, limits);
  CHECK(encoded.has_value(), "the existing native plan codec accepts explicitly missing geometry");
  if (!encoded) { return {}; }
  auto decoded = DecodeTransportPlan(*encoded, limits);
  CHECK(decoded && decoded->Paths.empty() && decoded->Bodies.empty() &&
            decoded->Coverage == TransportCoverage::Partial &&
            decoded->Omissions.size() == network.Provenance().Paths.size() && decoded->Network &&
            decoded->Network->Topology().Edges().size() == network.Topology().Edges().size(),
        "the real-source control is explicitly a network payload, never a generated full plan");
  return std::move(*encoded);
}
}

int main() {
  auto fields = CanonicalGroundFixture::LoadFields();
  CHECK(fields.size() == 4, "both original whole fields and both actual MVT publications load");
  std::vector<uint8_t> firstPublication;
  for (size_t which = 0; which < fields.size(); ++which) {
    const auto &field = fields[which];
    const auto start = std::chrono::steady_clock::now();
    BeginCppAllocationProbe();
    auto projection = ProjectGroundPortFieldNetwork(field);
    const auto allocations = EndCppAllocationProbe();
    const auto duration = std::chrono::steady_clock::now() - start;
    CHECK(projection.has_value(),
          "the actual qualified field produces its bounded functional network");
    if (!projection) { continue; }
    CheckOriginalRows(field, *projection);
    const auto &network = *projection->Network;
    const auto &work = projection->Work;
    CHECK(network.Topology().Edges().size() < field.Source().Source->Topology().Edges().size() &&
              network.CapacityBytes() < field.Source().Source->CapacityBytes(),
          "a field projection does not retain the complete original source snapshot");
    std::println("GROUND_NETWORK field={} source_edges={} edges={} nodes={} paths={} references={} "
                 "source_capacity_bytes={} output_capacity_bytes={} scratch_capacity_bytes={} "
                 "edge_lookups={} node_lookups={} reference_visits={} copied_edge_bytes={} "
                 "allocations={} requested_bytes={} projection_us={:.3f}",
                 which,
                 field.Source().Source->Topology().Edges().size(),
                 network.Topology().Edges().size(),
                 network.Topology().Nodes().size(),
                 network.Provenance().Paths.size(),
                 network.Provenance().References.size(),
                 field.Source().Source->CapacityBytes(),
                 work.OutputCapacityBytes,
                 work.ScratchCapacityBytes,
                 work.EdgeLookups,
                 work.NodeLookups,
                 work.ReferenceVisits,
                 work.CopiedEdgeBytes,
                 allocations.Calls,
                 allocations.Bytes,
                 std::chrono::duration<double, std::micro>(duration).count());
    if (which >= 2) {
      auto bytes = EncodeNetworkControl(*projection);
      CHECK(!bytes.empty(), "the actual projected provenance crosses the existing codec");
      if (which == 2) {
        firstPublication = std::move(bytes);
      } else {
        CHECK(bytes == firstPublication,
              "actual source republication preserves serialized content exactly");
      }
    }
  }
  return Report();
}

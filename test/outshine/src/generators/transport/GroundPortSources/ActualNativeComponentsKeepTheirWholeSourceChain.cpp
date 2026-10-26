#include "CanonicalGroundSourceFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "GroundPortSources.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ratio>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckComponent(const GroundPortSources &sources,
                    const CanonicalGroundFixture::Source &input,
                    size_t which) {
  const auto &seeds = input.Components[which];
  const auto first = sources.Chain(seeds.front());
  CHECK(first && first->Source == input.Snapshot && first->SourceEdges.size() == seeds.size() &&
            first->Nodes.size() == seeds.size() + 1,
        "actual normalized source demand reaches its entire five/six-span ground component");
  if (!first) { return; }
  const auto nodes = input.Snapshot->Topology().Nodes();
  const auto edges = input.Snapshot->Topology().Edges();
  CHECK(nodes[first->Nodes.front()].SourceNodeId < nodes[first->Nodes.back()].SourceNodeId,
        "the smaller original endpoint determines canonical chain orientation");
  std::vector<World::TransportEdgeId> selected;
  for (const auto edge : first->SourceEdges) { selected.push_back(edges[edge].Id); }
  std::ranges::sort(selected);
  CHECK(std::ranges::equal(selected, seeds),
        "no source span is substituted, omitted, duplicated or joined by coordinate proximity");
  for (size_t at = seeds.size(); at-- > 0;) {
    auto reverse = seeds[at];
    reverse.Direction = World::EdgeDirection::Reverse;
    const auto chain = sources.Chain(reverse);
    CHECK(
        chain && chain->Nodes == first->Nodes && chain->SourceEdges == first->SourceEdges &&
            chain->WidthM == first->WidthM && chain->MaximumGradient == first->MaximumGradient,
        "reversed core/seed/alias order retains exactly the same complete canonical source chain");
  }
  CHECK(first->Work.SelectedSegments == seeds.size() &&
            first->Work.IncidenceVisits == 2 * seeds.size() &&
            first->Work.NodesVisited == seeds.size() + 1,
        "whole-component work is linear in actual admitted spans and nodes");
  CHECK(std::ranges::any_of(first->SourceEdges,
                            [&](uint32_t edge) {
                              constexpr uint64_t kDoubleExactWordLimit = uint64_t{1} << 53;
                              return edges[edge].Id.PathId > kDoubleExactWordLimit;
                            }),
        "actual 64-bit source identity survives without double rounding");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto input = CanonicalGroundFixture::Load();
  CHECK(input.has_value(),
        input ? "the unchanged native capture is available" : input.error().c_str());
  if (!input) { return Report(); }
  CHECK(input->Snapshot->Topology().Nodes().size() == 4433 &&
            input->Snapshot->Topology().Edges().size() == 8623 &&
            input->Components[0].size() == 5 && input->Components[1].size() == 6 &&
            input->Components[2].size() == 6,
        "the exact same real source rings as the Python reference are used");
  const auto begin = std::chrono::steady_clock::now();
  BeginCppAllocationProbe();
  const auto sources = GroundPortSources::Make(input->Snapshot, input->Profiles, input->Unknown);
  const auto allocations = EndCppAllocationProbe();
  const auto elapsed = std::chrono::steady_clock::now() - begin;
  CHECK(sources.has_value(), "the native source snapshot admits its finite physical incidence");
  if (!sources) { return Report(); }
  CHECK(sources->Work().PhysicalSegments == 4743 && sources->Work().IncidenceEntries == 9486 &&
            sources->Work().IndexNodeLookups == 9486,
        "directed aliases are indexed once per physical source segment");
  CheckComponent(*sources, *input, 0);
  CheckComponent(*sources, *input, 1);
  const auto pending = sources->Chain(input->Components[2].front(), {.SelectedSegments = 591});
  CHECK(!pending && pending.error() == GroundPortSourceError::UnknownFrontier,
        "the actual 591-span open source component remains Pending before any DEM work");
  const auto openCap = sources->Chain(input->Components[2].front(), {.SelectedSegments = 590});
  CHECK(
      !openCap && openCap.error() == GroundPortSourceError::CapacityExceeded,
      "the six-span seed ring actually reaches 591 connected source spans before frontier refusal");
  const auto limited = sources->Chain(input->Components[0].front(), {.SelectedSegments = 4});
  CHECK(!limited && limited.error() == GroundPortSourceError::CapacityExceeded,
        "a four-span budget cannot publish a truncated five-span component");
  Note("actual native source incidence",
       std::chrono::duration<double, std::milli>(elapsed).count(),
       "ms");
  Note("actual native source index capacity",
       static_cast<double>(sources->Work().ContainerCapacityBytes),
       "B");
  Note("actual native source index C++ allocations",
       static_cast<double>(allocations.Calls),
       "calls");
  Note("actual native source index C++ allocation bytes",
       static_cast<double>(allocations.Bytes),
       "B");
  return Report();
}

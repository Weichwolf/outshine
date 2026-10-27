#include "Check.h"
#include "CppAllocationProbe.h"
#include "MvtGroundFieldScope.h"
#include "MvtTransportSnapshotInternal.h"
#include "ScopedGroundSourceFixture.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <print>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr World::TransportEdgeId kSeed{.PathId = 8409897393650800292ULL};

std::optional<MvtGroundFieldScope> Scope(const ScopedGroundFixture::Source &source,
                                         const MvtGroundFieldCandidateInputs &inputs,
                                         const MvtTransportSnapshot &snapshot) {
  auto candidate = source.Index.Candidate(kSeed);
  CHECK(candidate.has_value(), "the original finite component remains a structural candidate");
  if (!candidate) { return {}; }
  auto scope = BindMvtGroundFieldScope(snapshot, inputs, std::move(*candidate));
  CHECK(scope.has_value(),
        "the prepared exact facts lease still qualifies all actual field members");
  if (!scope) { return {}; }
  CHECK(scope->Work.OriginalMemberVisits == 0 && scope->Work.OriginalNodeLookups == 3 &&
            scope->Work.QualifiedMemberVisits == 6 && scope->Work.ContentMemberVisits == 6,
        "a field performs three indexed node lookups and visits only its six original members");
  return std::move(*scope);
}

void CheckAdmission(const ScopedGroundFixture::Source &source) {
  const auto members = source.Snapshot.SourceFacts->Members.size();
  for (const auto &limits : {MvtGroundFieldLimits{.Members = members - 1},
                             MvtGroundFieldLimits{.MemberVisits = members - 1},
                             MvtGroundFieldLimits{.Ways = 1},
                             MvtGroundFieldLimits{.ContainerBytes = 0}}) {
    auto candidate = source.Index.Candidate(kSeed);
    CHECK(candidate.has_value(), "a prepared input retains its original admission context");
    if (!candidate) { return; }
    BeginCppAllocationProbe();
    const auto failed =
        BindMvtGroundFieldScope(source.Snapshot, source.Inputs, std::move(*candidate), limits);
    const auto allocated = EndCppAllocationProbe();
    CHECK(!failed && failed.error() == MvtGroundFieldError::CapacityExceeded &&
              allocated.Calls == 0,
          "prepared indexes never bypass original snapshot/member or local storage caps");
  }
}

enum class SourceControl : uint8_t { OuterAmbiguity, AmbiguousSelected, MissingSelected };

void CheckChangedScope(const ScopedGroundFixture::Source &source,
                       const MvtGroundFieldScope &original,
                       const MvtTransportSnapshot &snapshot,
                       uint64_t node,
                       SourceControl control) {
  const auto inputs = BindMvtGroundFieldCandidateInputs(snapshot, *source.Field, source.Streets);
  auto candidate = source.Index.Candidate(kSeed);
  CHECK(inputs && candidate,
        "the immutable index records ambiguity without globally rejecting unrelated fields");
  if (!inputs || !candidate) { return; }
  const auto *native = snapshot.Network->Topology().FindNode(node);
  CHECK(native != nullptr,
        "the controlled original domain node belongs to the actual native snapshot");
  if (native == nullptr) { return; }
  const auto lookup = static_cast<size_t>(native - snapshot.Network->Topology().Nodes().data());
  const auto state = control != SourceControl::MissingSelected
                         ? MvtGroundFieldCandidateInputs::AmbiguousGroup
                         : MvtGroundFieldCandidateInputs::MissingGroup;
  CHECK(inputs->OriginalGroups()[lookup] == state,
        "prepared data distinguishes a locally ambiguous node from a missing original group");
  const auto result = BindMvtGroundFieldScope(snapshot, *inputs, std::move(*candidate));
  if (control == SourceControl::OuterAmbiguity) {
    CHECK(result && result->Field.Info.ContentBinding == original.Field.Info.ContentBinding &&
              result->Field.Info.Input.Content != original.Field.Info.Input.Content,
          "an unconsumed outer ambiguity preserves the actual finite field and its stable content");
  } else {
    const auto error = control == SourceControl::AmbiguousSelected
                           ? MvtGroundFieldError::InvalidBinding
                           : MvtGroundFieldError::IncompleteIncidence;
    CHECK(!result && result.error() == error,
          "selected ambiguous or missing original groups remain rejected before DEM");
  }
}

void CheckLocalAmbiguity(const ScopedGroundFixture::Source &source,
                         const MvtGroundFieldScope &original) {
  const auto &facts = *source.Snapshot.SourceFacts;
  const auto nodes = original.Field.Source.Source->Topology().Nodes();
  std::vector<uint64_t> selected;
  std::vector<uint32_t> selectedGroups;
  for (const auto at : original.Field.Source.Nodes) {
    selected.push_back(nodes[at].SourceNodeId);
    selectedGroups.push_back(source.Inputs.OriginalGroups()[at]);
  }
  const auto outside = std::ranges::find_if(facts.Members, [&](const auto &member) {
    return std::ranges::find(selectedGroups, member.NodeGroup) == selectedGroups.end();
  });
  CHECK(outside != facts.Members.end(),
        "the actual source includes original groups outside the field");
  if (outside == facts.Members.end()) { return; }
  const auto another = std::ranges::find_if(facts.Members, [&](const auto &member) {
    return member.NodeGroup != outside->NodeGroup &&
           std::ranges::find(selectedGroups, member.NodeGroup) == selectedGroups.end();
  });
  CHECK(another != facts.Members.end(),
        "a second actual outer group supplies an ambiguity control");
  if (another == facts.Members.end()) { return; }
  const auto row = static_cast<size_t>(another - facts.Members.begin());
  for (const auto control : {SourceControl::OuterAmbiguity,
                             SourceControl::AmbiguousSelected,
                             SourceControl::MissingSelected}) {
    auto changed = std::make_shared<MvtTransportSourceFacts>(facts);
    const uint64_t node =
        control == SourceControl::OuterAmbiguity ? outside->DomainNode : selected.front();
    if (control != SourceControl::MissingSelected) {
      changed->Members[row].DomainNode = node;
    } else {
      for (auto &member : changed->Members) {
        if (member.DomainNode == node) { member.DomainNode = outside->DomainNode; }
      }
    }
    MvtTransport::BindSourceFacts(*changed);
    auto snapshot = source.Snapshot;
    snapshot.SourceFacts = std::move(changed);
    CheckChangedScope(source, original, snapshot, node, control);
  }
}
}

int main() {
  auto source = ScopedGroundFixture::Load();
  CHECK(source.has_value(),
        "the unchanged pinned original MVT reaches actual native input preparation");
  if (!source) { return Report(); }
  const auto &inputs = source->Inputs;
  CHECK(inputs.OriginalGroups().size() == source->Snapshot.Network->Topology().Nodes().size() &&
            inputs.WayFacts().size() == source->Snapshot.SourceWayCount &&
            inputs.Ports().Work.OriginalMemberVisits ==
                source->Snapshot.SourceFacts->Members.size(),
        "one prepared candidate owns exact node/way tables and visits every original member once");
  auto original = Scope(*source, inputs, source->Snapshot);
  if (!original) { return Report(); }
  const auto *const groupStorage = inputs.OriginalGroups().data();
  const auto *const wayStorage = inputs.WayFacts().data();
  for (const unsigned request : {0U, 1U, 2U, 3U, 4U, 5U}) {
    (void)request;
    const auto again = Scope(*source, inputs, source->Snapshot);
    CHECK(again && again->Field.Info.Input.Content == original->Field.Info.Input.Content &&
              again->Field.Info.ContentBinding == original->Field.Info.ContentBinding &&
              inputs.OriginalGroups().data() == groupStorage &&
              inputs.WayFacts().data() == wayStorage,
          "repeated actual requests reuse the same exact immutable tables without changing either "
          "binding");
  }
  CheckAdmission(*source);
  CheckLocalAmbiguity(*source, *original);
  std::println("GROUND_PREPARED_INDEX original_members={} node_rows={} way_rows={} table_bytes={}",
               inputs.Ports().Work.OriginalMemberVisits,
               inputs.OriginalGroups().size(),
               inputs.WayFacts().size(),
               (inputs.OriginalGroups().size() + inputs.WayFacts().size()) * sizeof(uint32_t));
  return Report();
}

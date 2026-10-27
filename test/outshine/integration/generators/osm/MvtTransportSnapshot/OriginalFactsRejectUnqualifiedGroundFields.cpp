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
#include <span>
#include <stop_token>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr World::TransportEdgeId kSeed{.PathId = 8409897393650800292ULL};

void ExpectFailure(const ScopedGroundFixture::Source &source,
                   const MvtTransportSnapshot &snapshot,
                   MvtGroundFieldError expected) {
  const auto inputs = BindMvtGroundFieldCandidateInputs(snapshot, *source.Field, source.Streets);
  auto candidate = source.Index.Candidate(kSeed);
  CHECK(inputs && candidate, "negative local facts remain an explicitly unqualified candidate");
  if (!inputs || !candidate) { return; }
  const auto scoped = BindMvtGroundFieldScope(snapshot, *inputs, std::move(*candidate));
  CHECK(!scoped && scoped.error() == expected,
        "unqualified original incidence is rejected before a field or DEM request exists");
}

void CheckLocalFacts(const ScopedGroundFixture::Source &source) {
  const auto &original = *source.Snapshot.SourceFacts;
  const auto terminal = std::ranges::find_if(original.Members, [](const auto &member) {
    return member.Edge.PathId == kSeed.PathId && member.SourceTerminal;
  });
  CHECK(terminal != original.Members.end(), "the actual selected original terminal is retained");
  if (terminal == original.Members.end()) { return; }
  const auto group = terminal->NodeGroup;
  constexpr std::array expected{MvtGroundFieldError::IncompleteIncidence,
                                MvtGroundFieldError::IncompleteIncidence,
                                MvtGroundFieldError::Frontier,
                                MvtGroundFieldError::UnknownExit};
  for (const unsigned change : {0U, 1U, 2U, 3U}) {
    auto facts = std::make_shared<MvtTransportSourceFacts>(original);
    auto snapshot = source.Snapshot;
    snapshot.SourceFacts = facts;
    auto &selected = facts->Groups[group];
    if (change == 0) { selected.Complete = false; }
    if (change == 1) { ++selected.FilteredEndpointCount; }
    if (change == 2) { selected.Frontier = true; }
    if (change == 3) {
      auto members = std::span(facts->Members).subspan(selected.FirstMember, selected.MemberCount);
      for (auto &member : members) {
        if (member.Edge.PathId != kSeed.PathId) { member.SourceTerminal = false; }
      }
    }
    MvtTransport::BindSourceFacts(*facts);
    ExpectFailure(source, snapshot, expected[change]);
  }
  const auto way =
      std::ranges::find(original.Ways, terminal->StreetWay, &MvtTransportWayFacts::StreetWay);
  CHECK(way != original.Ways.end(), "the selected actual feature is resolved through its Way");
  if (way == original.Ways.end()) { return; }
  const auto &provenance = source.Snapshot.Network->Provenance();
  const auto path = std::ranges::lower_bound(
      provenance.Paths, kSeed.PathId, {}, &World::TransportPathSource::PathId);
  auto omitted = source.Snapshot;
  omitted.Omissions.push_back({.Source = provenance.References[path->FirstReference].Source,
                               .Feature = source.Field->Features()[way->Feature].ContentDigest,
                               .Reason = MvtTransportOmissionReason::MissingTopology});
  ExpectFailure(source, omitted, MvtGroundFieldError::MissingTopology);
  auto candidate = source.Index.Candidate(kSeed);
  CHECK(candidate.has_value(), "the old candidate remains available for stale-binding control");
  if (candidate) {
    const auto stale = BindMvtGroundFieldScope(omitted, source.Inputs, std::move(*candidate));
    CHECK(!stale && stale.error() == MvtGroundFieldError::InvalidBinding,
          "changed omission facts cannot reuse a previously bound field candidate");
  }
}

void CheckAdmission(const ScopedGroundFixture::Source &source) {
  const auto strict = BindMvtGroundPortInputs(source.Snapshot, *source.Field, source.Streets);
  CHECK(!strict && strict.error() == MvtGroundPortInputError::MissingIncidence &&
            source.Inputs.UnrepresentedIncidences() == 3,
        "all three outer Reason5 sources stay explicit and the original strict entry stays red");
  for (const unsigned change : {0U, 1U, 2U}) {
    auto snapshot = source.Snapshot;
    if (change == 0) { snapshot.SourceFacts.reset(); }
    if (change == 1) { ++snapshot.SourceGeneration; }
    BeginCppAllocationProbe();
    const auto result = BindMvtGroundFieldCandidateInputs(
        snapshot,
        *source.Field,
        source.Streets,
        {.OriginalMembers = change == 2 ? size_t{0} : size_t{65536}});
    const auto allocations = EndCppAllocationProbe();
    CHECK(!result && allocations.Calls == 0 &&
              result.error() == (change == 0   ? MvtGroundPortInputError::MissingFacts
                                 : change == 1 ? MvtGroundPortInputError::InvalidBinding
                                               : MvtGroundPortInputError::CapacityExceeded),
          "missing/stale facts and original-member caps refuse before normalization allocations");
  }
  for (const auto &limits : {MvtGroundFieldLimits{.Members = 0},
                             MvtGroundFieldLimits{.SelectedSegments = 0},
                             MvtGroundFieldLimits{.ContainerBytes = 0}}) {
    auto candidate = source.Index.Candidate(kSeed);
    if (!candidate) {
      CHECK(false, "actual ground candidate unavailable");
      return;
    }
    BeginCppAllocationProbe();
    const auto result =
        BindMvtGroundFieldScope(source.Snapshot, source.Inputs, std::move(*candidate), limits);
    const auto allocations = EndCppAllocationProbe();
    CHECK(!result && result.error() == MvtGroundFieldError::CapacityExceeded &&
              allocations.Calls == 0,
          "scope caps refuse before source-index or DEM allocations");
  }
  auto candidate = source.Index.Candidate(kSeed);
  auto differentFacts = source.Snapshot;
  differentFacts.SourceFacts =
      std::make_shared<const MvtTransportSourceFacts>(*source.Snapshot.SourceFacts);
  if (candidate) {
    const auto rebound =
        BindMvtGroundFieldScope(differentFacts, source.Inputs, std::move(*candidate));
    CHECK(!rebound && rebound.error() == MvtGroundFieldError::InvalidBinding,
          "a once-validated candidate cannot substitute a different original-facts lease");
  }
  auto corrupt = source.Snapshot;
  auto facts = std::make_shared<MvtTransportSourceFacts>(*corrupt.SourceFacts);
  facts->Digest[0] ^= 1;
  corrupt.SourceFacts = std::move(facts);
  const auto corrupted = BindMvtGroundFieldCandidateInputs(corrupt, *source.Field, source.Streets);
  CHECK(!corrupted && corrupted.error() == MvtGroundPortInputError::InvalidBinding,
        "the candidate validates the actual facts digest before it can be reused");
  candidate = source.Index.Candidate(kSeed);
  if (!candidate) { return; }
  candidate->MaximumGradient *= .5;
  const auto profile =
      BindMvtGroundFieldScope(source.Snapshot, source.Inputs, std::move(*candidate));
  CHECK(!profile && profile.error() == MvtGroundFieldError::MissingProfile,
        "caller gradient substitutions cannot replace original binary32 profile words");
  candidate = source.Index.Candidate(kSeed);
  if (!candidate) { return; }
  std::stop_source cancellation;
  (void)cancellation.request_stop();
  BeginCppAllocationProbe();
  const auto canceled = BindMvtGroundFieldScope(
      source.Snapshot, source.Inputs, std::move(*candidate), {}, cancellation.get_token());
  const auto allocations = EndCppAllocationProbe();
  CHECK(!canceled && canceled.error() == MvtGroundFieldError::Canceled && allocations.Calls == 0,
        "cancellation refuses the candidate before any allocation or DEM work");
}
}

int main() {
  auto source = ScopedGroundFixture::Load();
  CHECK(source.has_value(), "the actual pinned source supplies complete original local facts");
  if (!source) { return Report(); }
  const auto unknown = source->Inputs.Ports().UnknownNodes;
  CheckLocalFacts(*source);
  CheckAdmission(*source);
  source->Field.reset();
  source->Streets = {};
  auto candidate = source->Index.Candidate(kSeed);
  CHECK(candidate.has_value(), "the existing source index outlives the raw provider tables");
  if (candidate) {
    const auto result =
        BindMvtGroundFieldScope(source->Snapshot, source->Inputs, std::move(*candidate));
    CHECK(result && unknown == source->Inputs.Ports().UnknownNodes,
          "owned candidate/facts still qualify the whole finite field without raw OSM leases");
  }
  return Report();
}

#include "CanonicalGroundTerrainFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "GroundPortBodies.h"
#include "MvtGroundFieldScope.h"
#include "MvtTransportSnapshotInternal.h"
#include "ScopedGroundSourceFixture.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <ratio>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr World::TransportEdgeId kSeed{.PathId = 8409897393650800292ULL};

std::optional<MvtGroundFieldScope> Scope(const ScopedGroundFixture::Source &source,
                                         const MvtTransportSnapshot &snapshot) {
  auto candidate = source.Index.Candidate(kSeed);
  auto inputs = BindMvtGroundFieldCandidateInputs(snapshot, *source.Field, source.Streets);
  CHECK(candidate && inputs, "actual native source and exact original-facts lease qualify");
  if (!candidate || !inputs) { return {}; }
  const auto start = std::chrono::steady_clock::now();
  BeginCppAllocationProbe();
  auto scope = BindMvtGroundFieldScope(snapshot, *inputs, std::move(*candidate));
  const auto allocations = EndCppAllocationProbe();
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  CHECK(scope.has_value(), "all original members qualify before DEM or body construction");
  if (!scope) { return {}; }
  CHECK(scope->Work.ContentMemberVisits == scope->Work.QualifiedMemberVisits &&
            scope->Work.ContentMemberVisits == 6,
        "semantic binding visits exactly the six consumed original members, not all source facts");
  std::println("GROUND_CONTENT_SCOPE ms={} allocations={} requested_bytes={} content_members={}",
               elapsed,
               allocations.Calls,
               allocations.Bytes,
               scope->Work.ContentMemberVisits);
  return std::move(*scope);
}

std::optional<GroundPortField> Field(GroundPortFieldScope scope,
                                     std::shared_ptr<const GroundPortTerrain> terrain,
                                     double prior = 3) {
  auto request = GroundPortFieldRequest::PrepareScoped(std::move(scope));
  CHECK(request.has_value(), "the existing finite field request owns the qualified contents");
  if (!request) { return {}; }
  auto field = GroundPortField::Make(std::move(*request), std::move(terrain), prior);
  CHECK(field.has_value(), "actual pinned DEM and the explicit three-metre recipe produce a field");
  if (!field) { return {}; }
  return std::move(*field);
}

void CheckOriginalContents(const ScopedGroundFixture::Source &source,
                           const GroundPortFieldScopeInfo &original) {
  const auto &raw = *source.Snapshot.SourceFacts;
  const auto member = std::ranges::find(raw.Members, kSeed, &MvtTransportSourceMember::Edge);
  CHECK(member != raw.Members.end(), "the actual selected source has original semantic members");
  if (member == raw.Members.end()) { return; }
  for (const unsigned control : {0U, 1U, 2U}) {
    auto snapshot = source.Snapshot;
    auto facts = std::make_shared<MvtTransportSourceFacts>(raw);
    auto way = std::ranges::find(facts->Ways, member->StreetWay, &MvtTransportWayFacts::StreetWay);
    if (control == 0) {
      for (auto &row : facts->Ways) {
        row.Feature += 10000;
        row.Ring += 10000;
      }
      std::ranges::reverse(facts->Ways);
    } else {
      // Mutate consumed captured values, then issue a new exact integrity lease.
      way->EndStencil[control - 1].LongitudeDeg += .000001;
    }
    MvtTransport::BindSourceFacts(*facts);
    snapshot.SourceFacts = std::move(facts);
    const auto changed = Scope(source, snapshot);
    CHECK(changed && changed->Field.Info.Input.Content != original.Input.Content,
          "every controlled original-facts change still changes the exact V2 runtime binding");
    if (!changed) { continue; }
    CHECK((changed->Field.Info.ContentBinding == original.ContentBinding) == (control == 0),
          "local feature/ring/row addresses are excluded, but consumed original geometry is bound");
  }
}

void CheckBodies(const GroundPortField &first, const GroundPortField &second) {
  constexpr GroundPortBodyRecipe recipe{.ThicknessM = .3, .ApronM = 5};
  auto a = BuildGroundPortBodies(first, recipe);
  auto b = BuildGroundPortBodies(second, recipe, {.ContainerBytes = 67108864});
  CHECK(a && b, "both actual publications create complete original closed-body products");
  if (!a || !b) { return; }
  CHECK(a->ContentBinding == b->ContentBinding && a->Bodies.size() == b->Bodies.size() &&
            a->Scope && b->Scope && a->Scope->Input.Content != b->Scope->Input.Content,
        "persistent physical-body contents agree across actual generations and larger work caps");
  for (size_t at = 0; at < a->Bodies.size(); ++at) {
    const auto &x = a->Bodies[at];
    const auto &y = b->Bodies[at];
    CHECK(x.Source == y.Source && x.Geometry.Points == y.Geometry.Points &&
              x.Geometry.Faces == y.Geometry.Faces &&
              x.Geometry.WallParents == y.Geometry.WallParents &&
              x.Geometry.WallKinds == y.Geometry.WallKinds && x.FaceParents == y.FaceParents &&
              x.TerrainContacts == y.TerrainContacts,
          "equal content keys correspond to equal complete bodies and actual underside contacts");
  }
  for (const auto changedRecipe :
       {GroundPortBodyRecipe{.ThicknessM = .4, .ApronM = 5},
        GroundPortBodyRecipe{.ThicknessM = .3, .ApronM = 6},
        GroundPortBodyRecipe{.ThicknessM = .3, .ApronM = 5, .ReliefM = .1}}) {
    const auto changed = BuildGroundPortBodies(first, changedRecipe);
    CHECK(changed && changed->ContentBinding != a->ContentBinding,
          "actual thickness, apron and relief recipes each change the physical-body content key");
  }
  const auto content = a->ContentBinding;
  const auto anchor = a->Anchor;
  auto moved = ReframeGroundPortBodies(
      std::move(*a),
      {.LongitudeDeg = anchor.LongitudeDeg + .001, .LatitudeDeg = anchor.LatitudeDeg});
  CHECK(moved && moved->ContentBinding == content,
        "the consuming plan frame changes coordinates without changing physical content identity");
}
}

int main() {
  auto first = ScopedGroundFixture::Load();
  auto second = ScopedGroundFixture::Load(2);
  CHECK(first && second, "the same original MVT is actually accepted once and twice before ingest");
  if (!first || !second) { return Report(); }
  CHECK(
      first->Snapshot.SourceGeneration == 1 && second->Snapshot.SourceGeneration == 2 &&
          first->Snapshot.Network->SourceIdentity() == second->Snapshot.Network->SourceIdentity() &&
          first->Inputs.Ports().ProfileInput.Content ==
              second->Inputs.Ports().ProfileInput.Content &&
          first->Snapshot.SourceFacts->Digest != second->Snapshot.SourceFacts->Digest,
      "real republication preserves native contents and intentionally changes the V2 lease digest");
  auto a = Scope(*first, first->Snapshot);
  auto b = Scope(*second, second->Snapshot);
  CHECK(a && b, "both actual generations qualify the same original whole finite component");
  if (!a || !b) { return Report(); }
  CHECK(a->Field.Info.Input.Content != b->Field.Info.Input.Content &&
            a->Field.Info.ContentBinding == b->Field.Info.ContentBinding,
        "runtime integrity and stable semantic contents remain explicitly separate");
  CheckOriginalContents(*first, a->Field.Info);
  auto candidate = second->Index.Candidate(kSeed);
  if (candidate) {
    const auto stale =
        BindMvtGroundFieldScope(second->Snapshot, first->Inputs, std::move(*candidate));
    CHECK(!stale && stale.error() == MvtGroundFieldError::InvalidBinding,
          "equal persistent contents never authorize stale original-facts or source leases");
  }
  auto terrain = CanonicalGroundFixture::LoadTerrain();
  CHECK(terrain.has_value(), "the original pinned DEM is available without modifying any oracle");
  if (!terrain) { return Report(); }
  auto bound =
      GroundPortTerrain::Bind(terrain->Input, terrain->SourceDatum, terrain->HeightMapping);
  CHECK(bound.has_value(),
        "the existing actual raster is shared with its datum and sampler binding");
  if (!bound) { return Report(); }
  auto held = std::make_shared<const GroundPortTerrain>(std::move(*bound));
  auto prior = Field(a->Field, held, 3.5);
  auto x = Field(std::move(a->Field), held);
  auto y = Field(std::move(b->Field), held);
  first = std::unexpected(std::string{"raw source retired"});
  second = std::unexpected(std::string{"raw source retired"});
  CHECK(x && y && prior, "owned fields survive retirement of all original raw source producers");
  if (!x || !y || !prior) { return Report(); }
  const auto &firstScope = x->Scope();
  const auto &secondScope = y->Scope();
  CHECK(firstScope && secondScope && firstScope->ContentBinding == secondScope->ContentBinding &&
            firstScope->Input.Content != secondScope->Input.Content,
        "owned field products retain both stable content and distinct runtime provenance");
  CheckBodies(*x, *y);
  const auto original = BuildGroundPortBodies(*x, {.ThicknessM = .3, .ApronM = 5});
  const auto changed = BuildGroundPortBodies(*prior, {.ThicknessM = .3, .ApronM = 5});
  CHECK(original && changed && original->ContentBinding != changed->ContentBinding,
        "a real prior-recipe change is bound even when both original fields are feasible");
  return Report();
}

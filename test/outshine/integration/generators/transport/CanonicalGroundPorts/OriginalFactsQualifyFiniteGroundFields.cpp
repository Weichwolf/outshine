#include "CanonicalGroundTerrainFixture.h"
#include "CanonicalGroundMeshFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "GroundPortBodies.h"
#include "GroundPortField.h"
#include "GroundPortTerrain.h"
#include "MvtGroundFieldScope.h"
#include "ScopedGroundSourceFixture.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <print>
#include <ratio>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr std::array<World::TransportEdgeId, 2> kSeeds{
    {{.PathId = 8409897393650800292ULL}, {.PathId = 11909413124558356807ULL}}};

std::optional<GroundPortField> Field(ScopedGroundFixture::Source &source,
                                     const std::shared_ptr<const GroundPortTerrain> &terrain,
                                     World::TransportEdgeId seed,
                                     double maximumPriorM) {
  const auto strict = source.Index.Chain(seed);
  CHECK(!strict && strict.error() == GroundPortSourceError::UnknownFrontier,
        "the existing strict producer still rejects logical Unknown before DEM");
  auto candidate = source.Index.Candidate(seed);
  CHECK(candidate && candidate->SourceEdges.size() == 2 && candidate->Nodes.size() == 3,
        "the existing native index returns the actual whole two-span structural candidate");
  if (!candidate) { return {}; }
  const auto start = std::chrono::steady_clock::now();
  MvtGroundFieldLimits limits;
  if (seed == kSeeds[1]) {
    limits.ContainerBytes *= 2;
    limits.MemberVisits *= 2;
  }
  BeginCppAllocationProbe();
  auto scoped =
      BindMvtGroundFieldScope(source.Snapshot, source.Inputs, std::move(*candidate), limits);
  const auto allocations = EndCppAllocationProbe();
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  if (!scoped) { std::println("NATIVE_SCOPE_ERROR {}", static_cast<unsigned>(scoped.error())); }
  CHECK(scoped && scoped->Field.Info.Exits.size() == 2,
        "actual V2 Original/SourceTerminal facts qualify exactly both typed finite field exits");
  if (!scoped) { return {}; }
  std::println("NATIVE_SCOPE ms={} allocations={} requested_bytes={} original_visits={} "
               "qualified_visits={} coverage_tests={} container_capacity_bytes={}",
               elapsed,
               allocations.Calls,
               allocations.Bytes,
               scoped->Work.OriginalMemberVisits,
               scoped->Work.QualifiedMemberVisits,
               scoped->Work.CoverageTests,
               scoped->Work.ContainerCapacityBytes);
  CHECK(scoped->Work.QualifiedMemberVisits == 6 && scoped->Field.Info.Exits[0].Layer == 1 &&
            scoped->Field.Info.Exits[1].Layer == 1,
        "the actual two-span proof retains all six members and both Layer1 outer recipes");
  auto request = GroundPortFieldRequest::PrepareScoped(std::move(scoped->Field));
  CHECK(request.has_value(),
        "the existing owned request prepares the scoped original full-width demand");
  if (!request) { return {}; }
  auto made = GroundPortField::Make(std::move(*request), terrain, maximumPriorM);
  if (maximumPriorM == .5) {
    CHECK(!made, "the unchanged half-metre recipe remains infeasible on the actual new source");
    return {};
  }
  if (!made) {
    std::visit(
        [](auto error) {
          std::println("NATIVE_SCOPE_FIELD_ERROR {}", static_cast<unsigned>(error));
        },
        made.error());
  }
  CHECK(made && made->Scope() && made->Scope()->Exits.size() == 2,
        "the existing finite producer owns the actual qualified scope and whole 3m-recipe field");
  if (!made) { return {}; }
  return std::move(*made);
}
}

int main(int argc, char **argv) {
  auto source = ScopedGroundFixture::Load();
  if (!source) { std::println("SCOPED_SOURCE_ERROR {}", source.error()); }
  auto terrain = CanonicalGroundFixture::LoadTerrain();
  CHECK(source && terrain, "original actual V2 source and pinned original DEM load together");
  if (!source || !terrain) { return Report(); }
  const auto unknown = source->Inputs.Ports().UnknownNodes;
  auto bound =
      GroundPortTerrain::Bind(terrain->Input, terrain->SourceDatum, terrain->HeightMapping);
  CHECK(bound.has_value(), "the existing original terrain producer binds without resampling");
  if (!bound) { return Report(); }
  auto held = std::make_shared<const GroundPortTerrain>(std::move(*bound));
  (void)Field(*source, held, kSeeds[0], .5);
  auto first = Field(*source, held, kSeeds[0], 3);
  auto second = Field(*source, held, kSeeds[1], 3);
  CHECK(first && second, "both original orders produce a complete native 3m field");
  if (!first || !second) { return Report(); }
  const auto &firstScope = first->Scope();
  const auto &secondScope = second->Scope();
  CHECK(firstScope && secondScope, "both owned fields retain their qualified original scope");
  if (!firstScope || !secondScope) { return Report(); }
  CHECK(firstScope->Input.Content == secondScope->Input.Content &&
            first->Geometry().Surface.Points == second->Geometry().Surface.Points &&
            first->Geometry().Surface.Faces == second->Geometry().Surface.Faces &&
            first->Geometry().HeightsM == second->Geometry().HeightsM,
        "both original source request orders produce the same complete bound native 3m field");
  CanonicalGroundFixture::WriteMesh(first->Geometry(), 2, argc > 1 ? argv[1] : nullptr);
  CHECK(first->Geometry().Audit.NumericalQualityMet(),
        "the whole finite field retains the original continuous/finite numerical audit");
  const auto bodies = BuildGroundPortBodies(*first, {.ThicknessM = .3, .ApronM = 5});
  CHECK(bodies && bodies->Scope && bodies->Scope->Input.Content == firstScope->Input.Content &&
            bodies->Scope->Exits == firstScope->Exits && bodies->Bodies.size() == 2,
        "whole original source bodies carry the scoped input and typed exits toward the plan");
  const auto expectedScope = *firstScope;
  first.reset();
  second.reset();
  held.reset();
  CHECK(bodies && bodies->Scope && bodies->Scope->Input.Content == expectedScope.Input.Content &&
            bodies->Scope->Input.Revision == expectedScope.Input.Revision &&
            bodies->Scope->Exits == expectedScope.Exits,
        "scope identity and typed contacts remain owned after every field and fit is retired");
  CHECK(unknown == source->Inputs.Ports().UnknownNodes && !source->Index.Chain(kSeeds[0]),
        "the scoped geometry never changes original Unknown or creates a World connection");
  return Report();
}

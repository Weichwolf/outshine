#include "CanonicalGroundBodyContactsFixture.h"
#include "CanonicalGroundSourceFixture.h"
#include "CanonicalGroundTerrainFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "GeoCellId.h"
#include "GroundPortBodies.h"
#include "GroundPortField.h"
#include "GroundPortTerrain.h"
#include "MvtGroundFieldScope.h"
#include "ScopedGroundSourceFixture.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <print>
#include <ratio>
#include <stop_token>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
namespace Fixture = CanonicalGroundFixture;

std::optional<GroundPortBodies> MakeBodies(GroundPortFieldRequest request,
                                           std::shared_ptr<const GroundPortTerrain> terrain,
                                           double priorM) {
  auto field = GroundPortField::Make(std::move(request), std::move(terrain), priorM);
  CHECK(field.has_value(), "the unchanged original field supplies every finite face");
  if (!field) { return {}; }
  auto bodies = BuildGroundPortBodies(*field, {.ThicknessM = .3, .ApronM = 5, .ReliefM = 0});
  CHECK(bodies.has_value(), "whole source bodies exist before their original field is retired");
  if (!bodies) { return {}; }
  return std::move(*bodies);
}

std::vector<GroundPortBodies> Load() {
  std::vector<GroundPortBodies> bodies;
  auto source = Fixture::Load();
  auto terrain = Fixture::LoadTerrain();
  CHECK(source && terrain, "the pinned original affine/C1 sources and original DEM load");
  if (!source || !terrain) { return bodies; }
  auto bound =
      GroundPortTerrain::Bind(terrain->Input, terrain->SourceDatum, terrain->HeightMapping);
  auto index = GroundPortSources::Make(source->Snapshot, source->Profiles, source->Unknown);
  CHECK(bound && index, "the existing source index and original raster lease are reused");
  if (!bound || !index) { return bodies; }
  auto held = std::make_shared<const GroundPortTerrain>(std::move(*bound));
  for (size_t which = 0; which < 2; ++which) {
    auto request = GroundPortFieldRequest::Prepare(*index, source->Components[which].front());
    CHECK(request.has_value(), "original whole-component preparation remains strict");
    if (!request) { continue; }
    auto result = MakeBodies(std::move(*request), held, .5);
    if (result) { bodies.push_back(std::move(*result)); }
  }
  auto scoped = ScopedGroundFixture::Load();
  CHECK(scoped.has_value(), "the actual V2 source retains its separate exact snapshot binding");
  if (!scoped) { return bodies; }
  auto candidate = scoped->Index.Candidate({.PathId = 8409897393650800292ULL});
  CHECK(candidate.has_value(), "the actual additional finite ground candidate exists");
  if (!candidate) { return bodies; }
  auto scope =
      Osm::BindMvtGroundFieldScope(scoped->Snapshot, scoped->Inputs, std::move(*candidate));
  CHECK(scope.has_value(), "all actual typed exits are qualified before terrain work");
  if (!scope) { return bodies; }
  auto request = GroundPortFieldRequest::PrepareScoped(std::move(scope->Field));
  CHECK(request.has_value(), "the scoped original demand uses the existing owned request");
  if (!request) { return bodies; }
  auto result = MakeBodies(std::move(*request), std::move(held), 3);
  if (result) { bodies.push_back(std::move(*result)); }
  return bodies;
}

LongitudeLatitude PlanAnchor(LongitudeLatitude source, uint32_t offset) {
  constexpr uint32_t cells = uint32_t{1} << 15U;
  const auto x = static_cast<uint32_t>((source.LongitudeDeg + 180) / 360 * cells);
  const auto y = static_cast<uint32_t>((source.LatitudeDeg + 90) / 180 * cells);
  const auto bounds = GeoCellId{.Level = 15, .X = x + offset, .Y = y}.Bounds();
  CHECK(bounds.has_value(), "both plan anchors are actual canonical geographic core centres");
  if (!bounds) { return {}; }
  return {.LongitudeDeg = std::midpoint(bounds->WestDeg, bounds->EastDeg),
          .LatitudeDeg = std::midpoint(bounds->SouthDeg, bounds->NorthDeg)};
}

void CheckGeometry(const GroundPortBodies &original, const GroundPortBodies &reframed) {
  CHECK(reframed.Source == original.Source && reframed.Recipe == original.Recipe &&
            reframed.ProcedureAndTerrainBinding == original.ProcedureAndTerrainBinding &&
            reframed.Bodies.size() == original.Bodies.size(),
        "source/recipe/input ownership and all original physical bodies survive frame conversion");
  if (reframed.Bodies.size() != original.Bodies.size()) { return; }
  const auto source = TangentFrame::At(original.Anchor);
  const auto target = TangentFrame::At(reframed.Anchor);
  double maximumErrorM = 0;
  for (size_t which = 0; which < original.Bodies.size(); ++which) {
    const auto &a = original.Bodies[which];
    const auto &b = reframed.Bodies[which];
    CHECK(a.Source == b.Source && a.FaceParents == b.FaceParents &&
              a.Geometry.Faces == b.Geometry.Faces &&
              a.Geometry.WallParents == b.Geometry.WallParents &&
              a.Geometry.WallKinds == b.Geometry.WallKinds &&
              a.Geometry.SurfaceFaces == b.Geometry.SurfaceFaces &&
              a.Geometry.VolumeM3 == b.Geometry.VolumeM3 &&
              a.Geometry.Points.size() == b.Geometry.Points.size(),
          "reframing preserves the complete closed topology, actual source lineage and volume");
    if (a.Geometry.Points.size() != b.Geometry.Points.size()) { continue; }
    for (size_t point = 0; point < a.Geometry.Points.size(); ++point) {
      const auto &p = a.Geometry.Points[point];
      const auto &q = b.Geometry.Points[point];
      const auto before = source.ToEcefPosition({.EastM = p[0], .NorthM = p[1], .UpM = p[2]});
      const auto after = target.ToEcefPosition({.EastM = q[0], .NorthM = q[1], .UpM = q[2]});
      const double error =
          std::hypot(before[0] - after[0], before[1] - after[1], before[2] - after[2]);
      maximumErrorM = std::max(maximumErrorM, error);
      CHECK(error <= 1e-8, "every full native vertex remains at the same physical ECEF position");
    }
    for (size_t face = 0; face < b.Geometry.SurfaceFaces; ++face) {
      const auto fresh =
          BuildHeightSolidTerrainContact(b.Geometry,
                                         face,
                                         {.Effect = SurfaceGroundEffect::GroundContact,
                                          .ContactKey = b.Source.PathId,
                                          .ClearanceM = 0,
                                          .ReliefM = original.Recipe.ReliefM},
                                         original.Recipe.ApronM);
      CHECK(fresh && *fresh == b.TerrainContacts[face],
            "reused storage and fresh shared underside construction give identical full stamps");
    }
  }
  CHECK(reframed.Scope.has_value() == original.Scope.has_value(),
        "reframing neither invents nor drops original finite scope qualification");
  if (reframed.Scope && original.Scope) {
    CHECK(reframed.Scope->Input.Content == original.Scope->Input.Content &&
              reframed.Scope->Exits == original.Scope->Exits,
          "scope identity and actual typed exits stay in the original source contract");
  }
  std::println("GROUND_FRAME maximum_ecef_error_m={}", maximumErrorM);
}

void WriteDiagnostic(const GroundPortBodies &original,
                     const GroundPortBodies &reframed,
                     size_t which,
                     const char *path) {
  if (path == nullptr) { return; }
  const auto filename = std::format("{}.frame-{}.json", path, which);
  std::FILE *file = std::fopen(filename.c_str(), "wb");
  CHECK(file != nullptr, "explicit frame diagnostics have a writable destination");
  if (file == nullptr) { return; }
  std::print(file,
             R"({{"source_anchor":[{:.17g},{:.17g}],"target_anchor":[{:.17g},{:.17g}],"bodies":[)",
             original.Anchor.LongitudeDeg,
             original.Anchor.LatitudeDeg,
             reframed.Anchor.LongitudeDeg,
             reframed.Anchor.LatitudeDeg);
  for (size_t body = 0; body < original.Bodies.size(); ++body) {
    if (body != 0) { std::print(file, ","); }
    std::print(file, "{{\"source\":[");
    for (const bool target : {false, true}) {
      if (target) { std::print(file, "],\"target\":["); }
      const auto &points = (target ? reframed : original).Bodies[body].Geometry.Points;
      for (size_t at = 0; at < points.size(); ++at) {
        const auto point = points[at];
        std::print(
            file, "{}[{:.17g},{:.17g},{:.17g}]", at == 0 ? "" : ",", point[0], point[1], point[2]);
      }
    }
    std::print(file, "],\"faces\":[");
    const auto &faces = original.Bodies[body].Geometry.Faces;
    for (size_t at = 0; at < faces.size(); ++at) {
      const auto face = faces[at];
      std::print(file, "{}[{},{},{}]", at == 0 ? "" : ",", face[0], face[1], face[2]);
    }
    std::print(file, "]}}");
  }
  std::print(file, "]}}");
  CHECK(std::ferror(file) == 0, "the actual complete before/after frame data is written");
  CHECK(std::fclose(file) == 0, "the explicit frame diagnostic closes successfully");
}

void CheckFrame(const GroundPortBodies &original,
                LongitudeLatitude anchor,
                size_t which,
                const char *path) {
  auto owned = original;
  const size_t bytes = GroundPortBodyCapacityBytes(owned);
  const auto *bodyStorage = owned.Bodies.data();
  const auto *vertices = owned.Bodies.front().Geometry.Points.data();
  const auto *ring = owned.Bodies.front().TerrainContacts.front().RingEastNorthM.data();
  BeginCppAllocationProbe();
  const auto began = std::chrono::steady_clock::now();
  auto reframed = ReframeGroundPortBodies(std::move(owned), anchor, {.ContainerBytes = bytes});
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  const auto allocations = EndCppAllocationProbe();
  CHECK(reframed && allocations.Calls == 0 && allocations.Bytes == 0,
        "whole affine/C1/scoped bodies enter plan frames without new allocation or geometry copy");
  if (!reframed) {
    std::visit(
        [](auto error) { std::println("GROUND_FRAME_ERROR {}", static_cast<unsigned>(error)); },
        reframed.error());
    return;
  }
  CHECK(reframed->Bodies.data() == bodyStorage &&
            reframed->Bodies.front().Geometry.Points.data() == vertices &&
            reframed->Bodies.front().TerrainContacts.front().RingEastNorthM.data() == ring &&
            reframed->Work.ContainerCapacityBytes == bytes,
        "the exact admitted containers and original contact buffers are reused");
  CheckGeometry(original, *reframed);
  Fixture::CheckBodyContacts(*reframed, which, path);
  WriteDiagnostic(original, *reframed, which, path);
  std::println("GROUND_FRAME ms={} vertices={} contacts={} capacity_bytes={} allocations={}",
               elapsed,
               reframed->Work.ReframedVertices,
               reframed->Work.ReframedTerrainContacts,
               bytes,
               allocations.Calls);
}

void CheckGuards(const GroundPortBodies &original) {
  const auto target = PlanAnchor(original.Anchor, 1);
  const auto check = [&](GroundPortBodyBudget budget, const std::stop_token &stop) {
    auto input = original;
    BeginCppAllocationProbe();
    const auto result = ReframeGroundPortBodies(std::move(input), target, budget, stop);
    const auto allocations = EndCppAllocationProbe();
    CHECK(!result && allocations.Calls == 0,
          "failed admission/cancellation publishes no partial body and performs no allocation");
  };
  check({.Bodies = 0}, {});
  check({.SurfaceFaces = 0}, {});
  check({.ContainerBytes = 0}, {});
  check({.Solid = {.Vertices = 0}}, {});
  std::stop_source canceled;
  (void)canceled.request_stop();
  check({}, canceled.get_token());
  auto input = original;
  const auto *points = input.Bodies.front().Geometry.Points.data();
  BeginCppAllocationProbe();
  const auto unchanged = ReframeGroundPortBodies(std::move(input), original.Anchor);
  const auto allocations = EndCppAllocationProbe();
  CHECK(unchanged && allocations.Calls == 0 && unchanged->Work.ReframedVertices == 0 &&
            unchanged->Bodies.front().Geometry.Points.data() == points &&
            unchanged->Bodies.front().Geometry.Points == original.Bodies.front().Geometry.Points,
        "an identical frame retains the original point words and performs no transform");
  input = original;
  const auto invalid = ReframeGroundPortBodies(
      std::move(input),
      {.LongitudeDeg = std::numeric_limits<double>::quiet_NaN(), .LatitudeDeg = 0});
  CHECK(!invalid && std::holds_alternative<HeightSolidError>(invalid.error()) &&
            std::get<HeightSolidError>(invalid.error()) == HeightSolidError::InvalidInput,
        "invalid frames cannot publish a body");
}
}

int main(int argc, char **argv) {
  const char *diagnostic = argc > 1 ? argv[1] : nullptr;
  const auto bodies = Load();
  CHECK(bodies.size() == 3,
        "both original 5/6 components and the actual two-span field are retained");
  if (bodies.size() != 3) { return Report(); }
  for (size_t which = 0; which < bodies.size(); ++which) {
    CHECK(!bodies[which].Bodies.empty(), "every qualified source owns actual complete bodies");
    if (bodies[which].Bodies.empty()) { continue; }
    CheckGuards(bodies[which]);
    CheckFrame(bodies[which], PlanAnchor(bodies[which].Anchor, 0), which, nullptr);
    CheckFrame(bodies[which], PlanAnchor(bodies[which].Anchor, 1), which, diagnostic);
  }
  return Report();
}

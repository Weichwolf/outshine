#include "CanonicalGroundDemandFixture.h"
#include "CanonicalGroundMeshFixture.h"
#include "CanonicalGroundSourceFixture.h"
#include "CanonicalGroundTerrainFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "GroundPortField.h"
#include "GroundPortTerrain.h"
#include "HeightField.h"
#include "RoadPortFieldQuery.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <ratio>
#include <span>
#include <stop_token>
#include <tuple>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
namespace Fixture = CanonicalGroundFixture;

bool Same(const RoadPortFrameSample &a, const RoadPortFrameSample &b) {
  const auto words = [](const RoadPortFrameSample &value) {
    const auto &gate = value.Port.Gate;
    return std::tuple{gate.EastM,
                      gate.NorthM,
                      gate.GradeM,
                      gate.OutE,
                      gate.OutN,
                      gate.HalfWidthM,
                      gate.ShoulderM,
                      gate.ThicknessM,
                      value.Port.Plane.SlopeE,
                      value.Port.Plane.SlopeN,
                      value.LongitudinalGrade,
                      value.CrossfallRad,
                      value.RayIterations,
                      value.RayResidualM};
  };
  return words(a) == words(b);
}

void CheckSamples(const GroundPortField &product, std::span<const Fixture::Demand> demands) {
  std::vector<GroundPortFieldRequestPoint> points;
  for (const auto &demand : demands) {
    points.push_back({.Edge = demand.Edge, .Anchor = demand.Anchor});
  }
  const auto sampled = product.Sample(points);
  CHECK(sampled && sampled->Ports.size() == demands.size(),
        "the owned native product serves every actual geographic core port");
  if (!sampled) { return; }
  auto reference = RoadPortFieldQuery::Make(product.Fit().Field(), product.Domain().Faces);
  CHECK(reference.has_value(), "the previous native field query remains an independent consumer");
  if (!reference) { return; }
  for (size_t at = 0; at < demands.size(); ++at) {
    const auto &demand = demands[at];
    const auto height = product.Terrain().At(demand.Anchor);
    CHECK(height.has_value(), "each original request still has its actual bound DEM stencil");
    if (!height) { return; }
    const auto control =
        SampleRoadPortFrame(*reference,
                            {.SourceAnchor = product.Domain().Anchor,
                             .PortAnchor = demand.Anchor,
                             .OriginalSegment = demand.OriginalSegment,
                             .HalfWidthM = product.Source().WidthM / 2,
                             .InitialModelHeightM = *height,
                             .MaximumLongitudinalGrade = product.Source().MaximumGradient});
    CHECK(control && Same(sampled->Ports[at], *control),
          "product-derived source direction, frame, width and height equal the existing core "
          "consumer");
  }
  std::array<std::optional<GroundPortFieldSamples>, 2> parallel;
  {
    const std::jthread first([&] {
      auto own = product.Sample(points);
      if (own) { parallel[0] = std::move(*own); }
    });
    const std::jthread second([&] {
      auto own = product.Sample(points);
      if (own) { parallel[1] = std::move(*own); }
    });
  }
  for (const auto &own : parallel) {
    CHECK(own && own->Work.Field.Queries == sampled->Work.Field.Queries &&
              std::ranges::equal(own->Ports, sampled->Ports, Same),
          "concurrent consumers keep independent query work and identical immutable field samples");
  }
  std::ranges::reverse(points);
  for (auto &point : points) { point.Edge.Direction = World::EdgeDirection::Reverse; }
  const auto reverse = product.Sample(points, {.Ports = 2048, .ContainerBytes = 2097152});
  CHECK(reverse && reverse->Ports.size() == sampled->Ports.size(),
        "reverse request order and direction aliases retain the complete actual port batch");
  if (!reverse) { return; }
  for (size_t at = 0; at < sampled->Ports.size(); ++at) {
    CHECK(Same(sampled->Ports[at], reverse->Ports[reverse->Ports.size() - 1 - at]),
          "all port words stay exact across independent request order and admitted capacities");
  }
  BeginCppAllocationProbe();
  const auto empty = product.Sample({});
  const auto emptyCost = EndCppAllocationProbe();
  CHECK(empty && empty->Ports.empty() && empty->Work.ValidatedFaces == 0 && emptyCost.Calls == 0,
        "empty demand skips allocation, domain validation and terrain sampling");
  BeginCppAllocationProbe();
  const auto capped = product.Sample(points, {.Ports = 0});
  const auto counts = EndCppAllocationProbe();
  CHECK(!capped && counts.Calls == 0,
        "batch admission fails before output allocation or terrain sampling");
  points.front().Edge.PathId = 0;
  CHECK(!product.Sample(points), "a foreign edge cannot borrow this component's field or tangent");
  points.front().Edge = demands.front().Edge;
  points.front().Anchor.LongitudeDeg = std::numeric_limits<double>::infinity();
  CHECK(!product.Sample(points), "nonfinite geographic requests publish no fabricated frame");
  std::stop_source stop;
  stop.request_stop();
  CHECK(!product.Sample({}, {}, stop.get_token()),
        "even an empty batch honors explicit cancellation");
}

std::expected<GroundPortField, GroundPortFieldError>
MakeProduct(const Fixture::Source &source,
            const GroundPortSources &index,
            std::shared_ptr<const GroundPortTerrain> terrain,
            size_t which,
            bool reverse) {
  const auto seed = reverse ? source.Components[which].back() : source.Components[which].front();
  auto request = GroundPortFieldRequest::Prepare(index, seed);
  CHECK(request && request->TerrainQueries().size() == (which == 0 ? 36 : 58),
        "source-only preparation exposes exactly the original whole-width DEM demand");
  if (!request) { return std::unexpected(request.error()); }
  GroundPortFieldBudget budget;
  if (reverse) {
    budget.Ground.ContainerBytes *= 2;
    budget.Fit.Conditions *= 2;
    budget.Fit.Nonzeros *= 2;
    budget.Fit.Seconds *= 2;
    budget.Fit.Iterations *= 2;
    budget.Mesh.ContainerBytes *= 2;
  }
  BeginCppAllocationProbe();
  const auto began = std::chrono::steady_clock::now();
  auto product = GroundPortField::Make(std::move(*request), std::move(terrain), .5, budget);
  const auto counts = EndCppAllocationProbe();
  const double elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  CHECK(product.has_value(),
        "the actual admitted component becomes an owned qualified field product");
  if (!product) { return product; }
  CHECK(std::holds_alternative<RoadGroundAffine>(product->Fit().Value) == (which == 0),
        "the real service selects affine while the real non-affine path still requires C1");
  CHECK(product->Geometry().Surface.Points.size() == (which == 0 ? 36 : 1465) &&
            product->Geometry().Surface.Faces.size() == (which == 0 ? 34 : 1932),
        "the product preserves the full existing finite mesh rather than a coarse substitute");
  CHECK(product->Geometry().Audit.BadFaces == 0,
        "every emitted finite face meets the original independent gradient and contact audit");
  std::println(
      "GROUND_PRODUCT component={} reverse={} make_ms={} allocation_calls={} requested_bytes={} "
      "source_nodes={} source_segments={} observations={} mesh_vertices={} mesh_faces={}",
      which,
      reverse,
      elapsed,
      counts.Calls,
      counts.Bytes,
      product->Source().Nodes.size(),
      product->Source().SourceEdges.size(),
      product->Domain().Priors.size(),
      product->Geometry().Surface.Points.size(),
      product->Geometry().Surface.Faces.size());
  return product;
}

void CheckPending(const Fixture::Source &source,
                  const GroundPortSources &index,
                  const std::shared_ptr<const GroundPortTerrain> &terrain) {
  const auto open = GroundPortFieldRequest::Prepare(index, source.Components[2].front());
  CHECK(!open && std::get_if<GroundPortSourceError>(&open.error()) != nullptr &&
            std::get<GroundPortSourceError>(open.error()) == GroundPortSourceError::UnknownFrontier,
        "the actual open 591-segment source cannot request terrain or enter a partial fit");
  const auto capped =
      GroundPortFieldRequest::Prepare(index, source.Components[0].front(), {}, {.AxisPoints = 0});
  CHECK(!capped, "axis admission precedes geographic strip allocation");
  auto missing = GroundPortFieldRequest::Prepare(index, source.Components[0].front());
  CHECK(missing.has_value(), "the closed component may wait with owned source and strip demand");
  if (missing) {
    const auto absent = GroundPortField::Make(std::move(*missing), nullptr, .5);
    CHECK(!absent && std::get_if<GroundPortTerrainError>(&absent.error()) != nullptr &&
              std::get<GroundPortTerrainError>(absent.error()) ==
                  GroundPortTerrainError::MissingTerrain,
          "missing actual terrain remains an explicit unresolved input");
  }
  auto noMesh = GroundPortFieldRequest::Prepare(index, source.Components[0].front());
  CHECK(noMesh.has_value(), "an admitted request can be rejected by a later output budget");
  if (noMesh) {
    BeginCppAllocationProbe();
    const auto rejected =
        GroundPortField::Make(std::move(*noMesh), terrain, .5, {.Mesh = {.Vertices = 0}});
    const auto allocations = EndCppAllocationProbe();
    CHECK(!rejected && allocations.Calls == 0,
          "an impossible output cap rejects before DEM sampling, fitting or mesh allocation");
  }
  std::stop_source stop;
  stop.request_stop();
  CHECK(!GroundPortFieldRequest::Prepare(
            index, source.Components[0].front(), {}, {}, stop.get_token()),
        "cancellation cannot publish a source or DEM request");
}

void CheckTerrainGuards(const Fixture::Terrain &terrain) {
  BeginCppAllocationProbe();
  const auto capped = GroundPortTerrain::Bind(
      terrain.Input, terrain.SourceDatum, terrain.HeightMapping, {.Tiles = 0});
  const auto counts = EndCppAllocationProbe();
  CHECK(!capped && capped.error() == GroundPortTerrainError::CapacityExceeded && counts.Calls == 0,
        "terrain admission fails before hashing or metadata allocation");
  CHECK(!GroundPortTerrain::Bind(terrain.Input, {}, terrain.HeightMapping),
        "source datum cannot silently disappear from the original field binding");
  CHECK(!GroundPortTerrain::Bind(terrain.Input, terrain.SourceDatum, "unimplemented mapping"),
        "the producer cannot relabel raw source heights as an unimplemented model transform");
  const auto fallback = Ground::HeightField::Of(terrain.Tile[0], {}, true);
  CHECK(!GroundPortTerrain::Bind(fallback, terrain.SourceDatum, terrain.HeightMapping),
        "fallback terrain cannot masquerade as original qualified samples");
  auto blocks = std::vector(terrain.Input->Blocks().begin(), terrain.Input->Blocks().end());
  blocks.front().Nodes.push_back(0);
  const auto copied = Ground::HeightField::Of(terrain.Tile[0], std::move(blocks));
  CHECK(!GroundPortTerrain::Bind(copied, terrain.SourceDatum, terrain.HeightMapping),
        "copied or resampled raster storage cannot bypass the original immutable raster lease");
}
}

int main(int argc, char **argv) {
  struct RetainedProduct {
    std::shared_ptr<const GroundPortField> Field;
    std::vector<Fixture::Demand> Demands;
  };

  std::vector<RetainedProduct> retained;
  {
    auto source = Fixture::Load();
    auto terrain = Fixture::LoadTerrain();
    CHECK(source && terrain, "the unchanged real normalized source capture and original DEM load");
    if (!source || !terrain) { return Report(); }
    CheckTerrainGuards(*terrain);
    auto bound =
        GroundPortTerrain::Bind(terrain->Input, terrain->SourceDatum, terrain->HeightMapping);
    CHECK(bound && bound->Input().Content == terrain->Binding.Key &&
              bound->Work().CopiedRasterBytes == 0,
          "one actual original DEM lease reuses the established sampler/datum/model/raster binding "
          "exactly");
    if (!bound) { return Report(); }
    auto heldTerrain = std::make_shared<const GroundPortTerrain>(std::move(*bound));
    auto index = GroundPortSources::Make(source->Snapshot, source->Profiles, source->Unknown);
    CHECK(index.has_value(), "the actual native source index remains available");
    if (!index) { return Report(); }
    CheckPending(*source, *index, heldTerrain);
    for (size_t which = 0; which < 2; ++which) {
      auto first = MakeProduct(*source, *index, heldTerrain, which, false);
      auto second = MakeProduct(*source, *index, heldTerrain, which, true);
      if (!first || !second) { continue; }
      CHECK(first->ProcedureAndTerrainBinding() == second->ProcedureAndTerrainBinding() &&
                first->Geometry().Surface.Points == second->Geometry().Surface.Points &&
                first->Geometry().Surface.Faces == second->Geometry().Surface.Faces &&
                first->Geometry().HeightsM == second->Geometry().HeightsM,
            "independent source seed order and larger work caps preserve the whole bound product");
      if (argc > 1) { Fixture::WriteMesh(first->Geometry(), which, argv[1]); }
      auto product = std::make_shared<const GroundPortField>(std::move(*first));
      auto demands = Fixture::Demands(*source, source->Components[which]);
      CHECK(demands.size() == (which == 0 ? 2 : 8),
            "the two original components retain all ten native ports");
      retained.push_back({.Field = std::move(product), .Demands = std::move(demands)});
    }
  }
  for (const auto &held : retained) { CheckSamples(*held.Field, held.Demands); }
  return Report();
}

#include "CanonicalGroundSourceFixture.h"
#include "CanonicalGroundDemandFixture.h"
#include "CanonicalGroundTerrainFixture.h"
#include "CanonicalGroundMeshFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "GroundPortSources.h"
#include "RoadGroundStrip.h"
#include "RoadGroundAffine.h"
#include "RoadGroundFit.h"
#include "RoadGroundFacetAudit.h"
#include "RoadGroundMesh.h"
#include "RoadPortFieldFit.h"
#include "RoadPortFieldQuery.h"
#include "RoadPortFieldProgram.h"
#include "RoadPortFrame.h"
#include "TransportNetworkSnapshot.h"
#include "TransportPlanCodec.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <numeric>
#include <optional>
#include <utility>
#include <ratio>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

struct PortRecord {
  CanonicalGroundFixture::Demand Demand;
  RoadPortFrameSample Sample;
};

struct ComponentResult {
  std::vector<PortRecord> Ports;
  std::vector<double> Coefficients;
  std::optional<RoadGroundMesh> Geometry;
  std::optional<std::array<uint8_t, 32>> ProcedureAndTerrainBinding;
};

template <class Range> std::string Array(const Range &values) {
  std::string out = "[";
  for (const auto value : values) {
    if (out.size() > 1) { out += ','; }
    if constexpr (std::is_floating_point_v<decltype(value)>) {
      out += std::isfinite(value) ? std::format("{}", value) : "null";
    } else {
      out += std::format("{}", value);
    }
  }
  return out + "]";
}

void WriteFieldDiagnostics(const RoadGroundField &field,
                           const RoadPortFieldFit &fit,
                           size_t which,
                           const char *path) {
  if (path == nullptr) { return; }
  const auto program = BuildRoadPortFieldProgram(field.FitInput(.5), fit.Field, {}, {});
  CHECK(program.has_value(),
        "the exact native fitted LP can be exported for independent diagnosis");
  if (!program) { return; }
  std::vector<double> origins;
  for (const auto &plane : field.Planes) {
    origins.push_back(plane.Origin.EastM);
    origins.push_back(plane.Origin.NorthM);
  }
  std::vector<double> priors;
  for (const auto &prior : field.Priors) {
    priors.insert(priors.end(), {prior.At.EastM, prior.At.NorthM, prior.HeightM});
  }
  const auto &model = program->Program;
  std::vector<double> lower;
  std::vector<double> upper;
  for (const auto &variable : model.Variables) {
    lower.push_back(variable.Minimum);
    upper.push_back(variable.Maximum);
  }
  const std::string text =
      std::format("{{\"schema\":\"outshine/native-ground-lp-diagnostic/1\",\"anchor\":[{},{}],"
                  "\"datum_m\":{},\"plane_origins\":{},\"coefficients\":{},\"priors\":{},"
                  "\"variable_lower\":{},\"variable_upper\":{},\"column_offsets\":{},"
                  "\"row_indices\":{},\"values\":{},\"row_upper\":{}}}\n",
                  field.Anchor.LongitudeDeg,
                  field.Anchor.LatitudeDeg,
                  program->DatumM,
                  Array(origins),
                  Array(fit.Solver.Values),
                  Array(priors),
                  Array(lower),
                  Array(upper),
                  Array(model.Conditions.ColumnOffsets),
                  Array(model.Conditions.RowIndices),
                  Array(model.Conditions.Values),
                  Array(model.Upper));
  const std::string filename = std::format("{}.field-{}.json", path, which);
  std::FILE *file = std::fopen(filename.c_str(), "wb");
  CHECK(file != nullptr, "explicit full-LP diagnostics have a writable destination");
  if (file == nullptr) { return; }
  CHECK(std::fwrite(text.data(), 1, text.size(), file) == text.size(),
        "full-LP diagnostics are complete");
  CHECK(std::fclose(file) == 0, "full-LP diagnostics are closed successfully");
}

[[nodiscard]] auto Values(const RoadPortFrameSample &p) {
  const auto &g = p.Port.Gate;
  return std::tuple{g.EastM,
                    g.NorthM,
                    g.GradeM,
                    g.OutE,
                    g.OutN,
                    g.HalfWidthM,
                    g.ShoulderM,
                    g.ThicknessM,
                    p.Port.Plane.SlopeE,
                    p.Port.Plane.SlopeN,
                    p.LongitudinalGrade,
                    p.CrossfallRad,
                    p.RayIterations,
                    p.RayResidualM};
}

std::vector<PortRecord> Ports(const RoadPortField &fit,
                              const RoadGroundField &field,
                              std::span<const CanonicalGroundFixture::Demand> demands,
                              const CanonicalGroundFixture::Terrain &terrain,
                              const GroundPortSourceChain &chain) {
  auto query = RoadPortFieldQuery::Make(fit, field.Faces);
  CHECK(query.has_value(), "the fitted actual original domain is available to its port consumer");
  if (!query) { return {}; }
  std::vector<PortRecord> result;
  for (const auto &demand : demands) {
    const auto height = terrain.At(demand.Anchor);
    CHECK(height.has_value(), "each port ray starts from its bound original DEM sample");
    if (!height) { return {}; }
    const auto port = SampleRoadPortFrame(*query,
                                          {.SourceAnchor = field.Anchor,
                                           .PortAnchor = demand.Anchor,
                                           .OriginalSegment = demand.OriginalSegment,
                                           .HalfWidthM = chain.WidthM / 2,
                                           .InitialModelHeightM = *height,
                                           .MaximumLongitudinalGrade = chain.MaximumGradient});
    CHECK(port.has_value(),
          "actual geographic core demand produces a qualified own-ENU canonical port");
    if (!port) { return {}; }
    result.push_back({.Demand = demand, .Sample = *port});
  }
  return result;
}

void CheckCoarseFacets(const RoadGroundField &source, const RoadPortField &field) {
  PlanarMesh mesh;
  std::vector<double> heights;
  for (const auto &prior : source.Priors) {
    const auto sampled = field.Evaluate(prior.Patch, prior.At);
    CHECK(sampled.has_value(), "every original facet vertex samples the fitted analytic field");
    if (!sampled) { return; }
    mesh.Points.push_back({prior.At.EastM, prior.At.NorthM});
    heights.push_back(sampled->HeightM);
  }
  mesh.Faces = source.Triangles;
  mesh.FaceParents.resize(mesh.Faces.size());
  std::ranges::iota(mesh.FaceParents, uint32_t{0});
  const auto began = std::chrono::steady_clock::now();
  const auto audit = AuditRoadGroundFacets(source, field, mesh, heights);
  Note("actual coarse finite audit elapsed",
       std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
       "ms");
  CHECK(audit.has_value(), "both actual coarse products admit independent finite facet checks");
  if (!audit) { return; }
  Note("actual coarse finite bad faces", static_cast<double>(audit->BadFaces), "faces");
  Note("actual coarse chord bound", audit->MaximumHeightChordErrorM, "m");
  Note("actual coarse finite normalized grade", audit->MaximumNormalizedFiniteGrade, "ratio");
  Note("actual coarse contact gradient jump", audit->MaximumContactGradientJump, "m/m");
  Note("actual coarse shared contact segments",
       static_cast<double>(audit->SharedContactSegments),
       "segments");
  Note("actual coarse facet audit workspace",
       static_cast<double>(audit->ContainerCapacityBytes),
       "B");
}

std::optional<std::array<uint8_t, 32>>
BindProcedureAndTerrain(const TransportInputReference &procedure,
                        const CanonicalGroundFixture::Terrain &terrain) {
  std::array inputs{procedure,
                    TransportInputReference{.DatasetId = "outshine/terrain-bilinear-view",
                                            .Version = "1",
                                            .Revision = "",
                                            .ItemId = "original-raster",
                                            .Content = terrain.Binding.Key}};
  const auto binding = TransportPlanInputBinding(inputs, 4096);
  CHECK(binding.has_value(),
        "the actual solver procedure and sampled DEM identity compose through the existing codec");
  return binding ? std::make_optional(*binding) : std::nullopt;
}

ComponentResult CheckSelected(const RoadGroundField &field,
                              std::span<const CanonicalGroundFixture::Demand> demands,
                              const CanonicalGroundFixture::Terrain &terrain,
                              const GroundPortSourceChain &chain,
                              size_t which,
                              bool reverse,
                              const char *path) {
  RoadGroundBudget groundBudget;
  RoadProfiles::HeightFitBudget smoothBudget;
  if (reverse) {
    groundBudget.ContainerBytes *= 2;
    smoothBudget.Conditions *= 2;
    smoothBudget.Nonzeros *= 2;
    smoothBudget.Seconds *= 2;
    smoothBudget.Iterations *= 2;
  }
  const auto begin = std::chrono::steady_clock::now();
  BeginCppAllocationProbe();
  const auto selected = FitRoadGround(field, .5, groundBudget, smoothBudget);
  const auto allocations = EndCppAllocationProbe();
  Note("actual automatic fit elapsed",
       std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count(),
       "ms");
  Note("actual automatic fit C++ allocations", static_cast<double>(allocations.Calls), "calls");
  Note("actual automatic fit C++ requested bytes", static_cast<double>(allocations.Bytes), "B");
  CHECK(selected.has_value(), "both real components consume the native affine-first selection");
  if (!selected) { return {}; }
  CHECK(std::holds_alternative<RoadGroundAffine>(selected->Value) == (which == 0),
        "the actual service uses affine while the actual non-affine path requires C1");
  std::vector<double> coefficients;
  if (const auto *smooth = std::get_if<RoadPortFieldFit>(&selected->Value)) {
    coefficients = smooth->Solver.Values;
    CHECK(smooth->MaximumPriorErrorM <= .5 && smooth->MaximumNormalizedGrade <= 1,
          "the selected actual C1 field preserves the original quality requirements");
  } else {
    const auto sample = selected->Field().Evaluate(0, {});
    CHECK(sample.has_value(), "a shared affine plane retains its defining height and gradient");
    if (sample) {
      coefficients = {sample->HeightM, sample->Gradient.SlopeE, sample->Gradient.SlopeN};
    }
  }
  const auto selectedPath = path == nullptr ? std::string{} : std::format("{}.selected", path);
  auto geometry = CanonicalGroundFixture::CheckFiniteMesh(
      field, selected->Field(), which, reverse, path == nullptr ? nullptr : selectedPath.c_str());
  if (which == 0 && geometry) {
    CHECK(
        geometry->Surface.Points.size() == field.Vertices.size() &&
            geometry->Surface.Faces == field.Triangles,
        "the selected shared plane needs no parameter subdivision and retains every original face");
  }
  return {.Ports = Ports(selected->Field(), field, demands, terrain, chain),
          .Coefficients = std::move(coefficients),
          .Geometry = std::move(geometry),
          .ProcedureAndTerrainBinding = BindProcedureAndTerrain(selected->RecipeInput, terrain)};
}

void CheckSame(const ComponentResult &forward, const ComponentResult &backward, size_t which) {
  CHECK(forward.ProcedureAndTerrainBinding &&
            forward.ProcedureAndTerrainBinding == backward.ProcedureAndTerrainBinding,
        "the actual procedure and DEM binding remain exact across independent request order "
        "and admitted work caps");
  CHECK(forward.Geometry && backward.Geometry &&
            forward.Geometry->Surface.Points == backward.Geometry->Surface.Points &&
            forward.Geometry->Surface.Faces == backward.Geometry->Surface.Faces &&
            forward.Geometry->Surface.FaceParents == backward.Geometry->Surface.FaceParents &&
            forward.Geometry->Surface.PointRemap == backward.Geometry->Surface.PointRemap &&
            forward.Geometry->HeightsM == backward.Geometry->HeightsM,
        "independent reversed source/core order and different admitted output capacities "
        "produce exactly the same complete finite geometry");
  CHECK(forward.Ports.size() == (which == 0 ? 2 : 8) &&
            forward.Ports.size() == backward.Ports.size(),
        "both actual components produce all ten ports through native geographic core selection");
  if (forward.Ports.size() != backward.Ports.size()) { return; }
  for (size_t at = 0; at < forward.Ports.size(); ++at) {
    CHECK(Values(forward.Ports[at].Sample) ==
              Values(backward.Ports[backward.Ports.size() - 1 - at].Sample),
          "independent reversed source/core order and admitted work budgets produce exactly equal "
          "ports");
  }
  CHECK(forward.Coefficients == backward.Coefficients,
        "all field coefficients remain exact across independent request order and admitted caps");
}

void CheckSelectedC1(const ComponentResult &forced, const ComponentResult &selected) {
  CHECK(forced.Coefficients == selected.Coefficients,
        "actual non-affine selection retains the exact defining coefficients of the C1 control");
  for (size_t at = 0; at < forced.Ports.size() && at < selected.Ports.size(); ++at) {
    CHECK(Values(forced.Ports[at].Sample) == Values(selected.Ports[at].Sample),
          "all actual selected C1 ports remain exact to the forced C1 control");
  }
}

std::array<ComponentResult, 2> CheckField(const CanonicalGroundFixture::Source &source,
                                          const GroundPortSources &sources,
                                          const CanonicalGroundFixture::Terrain &terrain,
                                          size_t which,
                                          bool reverse,
                                          const char *path) {
  const auto chain =
      sources.Chain(reverse ? source.Components[which].back() : source.Components[which].front());
  CHECK(chain.has_value(), "actual normalized source closure supplies the full height-field input");
  if (!chain) { return {}; }
  const auto nodes = chain->Source->Topology().Nodes();
  std::vector<LongitudeLatitude> geographic;
  for (const auto at : chain->Nodes) {
    geographic.push_back(
        {.LongitudeDeg = nodes[at].LongitudeDeg, .LatitudeDeg = nodes[at].LatitudeDeg});
  }
  const auto begin = std::chrono::steady_clock::now();
  const RoadGroundBudget budget{.TerrainSamples = reverse ? size_t{128} : size_t{4096}};
  const auto strip =
      PrepareRoadGroundStrip(geographic, chain->WidthM, chain->MaximumGradient, budget);
  CHECK(strip.has_value(),
        "actual source geometry prepares the entire bounded full-width DEM request");
  if (!strip) { return {}; }
  std::vector<double> heights;
  for (const auto query : strip->TerrainQueries) {
    const auto height = terrain.At(query);
    CHECK(height.has_value(), "every full-width observation has its original bound DEM stencil");
    if (!height) { return {}; }
    const auto reference = terrain.ReferenceAt(query);
    CHECK(reference.has_value(),
          "the unchanged reference stencil remains available for comparison");
    if (reference) {
      CHECK_NEAR(*height,
                 *reference,
                 1e-7,
                 "m",
                 "native logarithmic Mercator and reference asinh projections retain the same "
                 "physical bilinear DEM contract");
    }
    heights.push_back(*height);
  }
  const auto field = BindRoadGroundField(*strip, heights, budget);
  CHECK(field.has_value(), "actual DEM heights reach the common native field in the source frame");
  if (!field) { return {}; }
  CHECK(field->Work.MergedPlanes == 0 && field->Priors.size() == strip->TerrainQueries.size() &&
            field->Faces.size() == strip->Triangles.size(),
        "the actual C1 domains retain every original observation and coarse face");
  const auto affineBegin = std::chrono::steady_clock::now();
  const auto affine = FitRoadGroundAffine(*field, budget);
  Note("actual affine control elapsed",
       std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - affineBegin)
           .count(),
       "ms");
  CHECK(affine && (which == 0 ? affine->MaximumPriorErrorM <= .5 : affine->MaximumPriorErrorM > .5),
        "the same affine control admits the real service and requires C1 for the real non-affine "
        "path");
  if (affine) { Note("actual affine original prior error", affine->MaximumPriorErrorM, "m"); }
  BeginCppAllocationProbe();
  const auto fitted = FitRoadPortField(field->FitInput(.5), {});
  const auto fitAllocations = EndCppAllocationProbe();
  Note("actual forced C1 fit C++ allocations", static_cast<double>(fitAllocations.Calls), "calls");
  Note("actual forced C1 fit C++ requested bytes", static_cast<double>(fitAllocations.Bytes), "B");
  CHECK(fitted.has_value(),
        "both actual DEM components admit the native conservative C1/HiGHS fit");
  if (!fitted) { return {}; }
  CHECK(fitted->MaximumPriorErrorM <= .5 && fitted->MaximumNormalizedGrade <= 1,
        "the original half-metre prior and continuous physical gradient requirements hold");
  Note("actual source/DEM/C1 elapsed",
       std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count(),
       "ms");
  Note("actual source/DEM/C1 original prior error", fitted->MaximumPriorErrorM, "m");
  CHECK(heights.size() == (which == 0 ? 36 : 58),
        "the native fields retain exactly the same 94 original full-width DEM stencils");
  CheckCoarseFacets(*field, fitted->Field);
  auto geometry =
      CanonicalGroundFixture::CheckFiniteMesh(*field, fitted->Field, which, reverse, path);

  auto demands = CanonicalGroundFixture::Demands(source, source.Components[which]);
  if (reverse) { std::ranges::reverse(demands); }
  if (!reverse) { WriteFieldDiagnostics(*field, *fitted, which, path); }
  Note("actual C1 procedure encoded inputs", static_cast<double>(fitted->Work.ProcedureBytes), "B");
  ComponentResult forced{.Ports = Ports(fitted->Field, *field, demands, terrain, *chain),
                         .Coefficients = fitted->Solver.Values,
                         .Geometry = std::move(geometry),
                         .ProcedureAndTerrainBinding =
                             BindProcedureAndTerrain(fitted->RecipeInput, terrain)};
  return {std::move(forced), CheckSelected(*field, demands, terrain, *chain, which, reverse, path)};
}

void WriteDiagnostics(std::span<const PortRecord> ports, const char *path) {
  if (path == nullptr) { return; }
  std::string text;
  for (const auto &record : ports) {
    const auto &d = record.Demand;
    const auto &p = record.Sample;
    text +=
        std::format("{} {} {} {} {} {} {:.17g} {:.17g} {:.17g} {:.17g} {:.17g} {:.17g} {:.17g}\n",
                    d.Edge.PathId,
                    d.Edge.SegmentOrdinal,
                    d.Boundary.Level,
                    static_cast<unsigned>(d.Boundary.Axis),
                    d.Boundary.Line,
                    d.Boundary.Segment,
                    p.Port.Gate.GradeM,
                    p.Port.Plane.SlopeE,
                    p.Port.Plane.SlopeN,
                    p.Port.Gate.OutE,
                    p.Port.Gate.OutN,
                    p.LongitudinalGrade,
                    p.CrossfallRad);
  }
  std::FILE *file = std::fopen(path, "wb");
  CHECK(file != nullptr, "explicit port diagnostics have a writable destination");
  if (file == nullptr) { return; }
  CHECK(std::fwrite(text.data(), 1, text.size(), file) == text.size(),
        "port diagnostics are complete");
  CHECK(std::fclose(file) == 0, "port diagnostics are closed successfully");
}
}

int main(int argc, char **argv) {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto source = CanonicalGroundFixture::Load();
  const auto terrain = CanonicalGroundFixture::LoadTerrain();
  CHECK(source && terrain, "original normalized source, recipe and DEM input pins are available");
  if (!source || !terrain) { return Report(); }
  CHECK(terrain->Binding.Fields == 1 && terrain->Binding.CopiedRasterBytes == 0 &&
            terrain->Input->Qualified() && terrain->Input->Blocks()[0].Nodes.empty(),
        "the original decoder raster reaches the bound production bilinear sampler through its "
        "existing lease without a copy");
  CHECK(
      !terrain->SourceDatum.empty() && !terrain->HeightMapping.empty(),
      "the actual source datum and explicit model mapping remain bound without a geoid invention");
  const auto sources = GroundPortSources::Make(source->Snapshot, source->Profiles, source->Unknown);
  CHECK(sources.has_value(),
        "actual physical source incidence is prepared once for both components");
  if (sources) {
    std::vector<PortRecord> ports;
    std::vector<PortRecord> selectedPorts;
    for (size_t which = 0; which < 2; ++which) {
      const auto forward =
          CheckField(*source, *sources, *terrain, which, false, argc > 1 ? argv[1] : nullptr);
      const auto backward = CheckField(*source, *sources, *terrain, which, true, nullptr);
      for (size_t method = 0; method < forward.size(); ++method) {
        CheckSame(forward[method], backward[method], which);
      }
      if (which == 1) { CheckSelectedC1(forward[0], forward[1]); }
      ports.insert(ports.end(), forward[0].Ports.begin(), forward[0].Ports.end());
      selectedPorts.insert(selectedPorts.end(), forward[1].Ports.begin(), forward[1].Ports.end());
    }
    WriteDiagnostics(ports, argc > 1 ? argv[1] : nullptr);
    if (argc > 1) {
      const auto selectedPath = std::format("{}.selected", argv[1]);
      WriteDiagnostics(selectedPorts, selectedPath.c_str());
    }
  }
  return Report();
}

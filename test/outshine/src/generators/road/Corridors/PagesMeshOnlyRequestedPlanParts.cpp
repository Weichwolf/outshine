#include "Corridors.h"
#include "ProfiledRoadMesher.h"
#include "Check.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

class CountedMesher final : public RoadMesher {
public:
  mutable size_t Sweeps = 0;
  mutable size_t Junctions = 0;
  std::stop_source *CancelAfterSweep = nullptr;

  RoadMeshingStats
  Sweep(std::span<const RoadStation> along, RoadSweep how, RoadMeshBuffers &into) const override {
    ++Sweeps;
    const auto result = Native_.Sweep(along, how, into);
    if (CancelAfterSweep != nullptr) { (void)CancelAfterSweep->request_stop(); }
    return result;
  }

  void Junction(std::span<const RoadGate> gates,
                RoadPlane plane,
                const Vec3f &wears,
                RoadMeshBuffers &into) const override {
    ++Junctions;
    Native_.Junction(gates, plane, wears, into);
  }

private:
  ProfiledRoadMesher Native_;
};

std::unique_ptr<Corridors::Job> Plan(const Corridors &corridors, const Corridors::Site &site) {
  auto job = Corridors::Begin(site);
  for (size_t slice = 0; slice < 2048; ++slice) {
    const auto ready = corridors.AdvancePlan(*job, site, 1, 1);
    if (!ready) { return {}; }
    if (*ready) { return job; }
  }
  return {};
}

template <class T> bool SameWords(std::span<const T> left, std::span<const T> right) {
  return std::ranges::equal(std::as_bytes(left), std::as_bytes(right));
}

bool SameMesh(const RoadMeshBuffers &mesh, const Geometry &geometry, int part) {
  return SameWords<float>(mesh.PositionM, geometry.positionsOf(part)) &&
         SameWords<float>(mesh.NormalM, geometry.normalsOf(part)) &&
         SameWords<float>(mesh.ColourRgba, geometry.coloursOf(part)) &&
         SameWords<uint32_t>(mesh.Index, geometry.trianglesOf(part));
}

void CheckSelection(const Corridors::Job &plan,
                    const Corridors::Site &site,
                    std::span<const RoadPagePart> parts) {
  using namespace outshine::Test;
  const auto interval = std::ranges::find(parts, RoadPagePartKind::Interval, &RoadPagePart::Kind);
  const auto junction = std::ranges::find(parts, RoadPagePartKind::Junction, &RoadPagePart::Kind);
  CHECK(interval != parts.end() && junction != parts.end(),
        "fixture plans intervals and a junction");
  if (interval == parts.end() || junction == parts.end()) { return; }
  const std::array<RoadPagePart, 1> one{*interval};
  const auto recipe = Corridors::PageRecipe(plan, site, one);
  CHECK(recipe && recipe->Intervals.size() == 1 && recipe->Junctions.empty(),
        "a page owns only its requested whole interval");
  CountedMesher selected;
  const auto product =
      recipe
          ? MeshRoadPage(selected, *recipe)
          : std::expected<RoadPageGeometry, std::string_view>{std::unexpected("missing interval")};
  CHECK(product && !product->Mesh.Index.empty() && selected.Sweeps == 1 && selected.Junctions == 0,
        "unrequested intervals and junctions cause no mesh work");
  const auto node = Corridors::PageRecipe(plan, site, std::array{*junction});
  CountedMesher body;
  const auto bodyProduct =
      node ? MeshRoadPage(body, *node)
           : std::expected<RoadPageGeometry, std::string_view>{std::unexpected("missing junction")};
  CHECK(node && node->Intervals.empty() && node->Junctions.size() == 1 &&
            node->Junctions[0].Gates.size() >= 2 && bodyProduct && body.Sweeps == 0 &&
            body.Junctions == 1,
        "a junction is an independently generated whole native piece");
  CHECK(!Corridors::PageRecipe(plan, site, std::array{*interval, *interval}),
        "duplicate part requests cannot duplicate native geometry");
  CHECK(!Corridors::PageRecipe(
            plan,
            site,
            std::array{RoadPagePart{.Kind = RoadPagePartKind::Interval, .Index = UINT32_MAX}}),
        "an unknown interval is not silently omitted");
  const auto empty = Corridors::PageRecipe(plan, site, {});
  CountedMesher none;
  const auto emptyProduct = empty ? MeshRoadPage(none, *empty)
                                  : std::expected<RoadPageGeometry, std::string_view>{
                                        std::unexpected("missing empty recipe")};
  CHECK(emptyProduct && emptyProduct->Mesh.Index.empty() && none.Sweeps == 0 && none.Junctions == 0,
        "no demand requires no mesh work");
}

std::array<int64_t, 2> CellOf(const auto &points) {
  std::array<double, 2> low{points.front().EastM, points.front().NorthM};
  auto high = low;
  for (const auto &point : points) {
    low[0] = std::min(low[0], point.EastM);
    low[1] = std::min(low[1], point.NorthM);
    high[0] = std::max(high[0], point.EastM);
    high[1] = std::max(high[1], point.NorthM);
  }
  return {static_cast<int64_t>(std::floor(std::midpoint(low[0], high[0]) / 64)),
          static_cast<int64_t>(std::floor(std::midpoint(low[1], high[1]) / 64))};
}

void CheckLayout(const Corridors::Job &plan,
                 const Corridors::Site &site,
                 std::span<const RoadPagePart> original) {
  using namespace outshine::Test;
  const auto pages = Corridors::PlannedPages(plan, 64, 2);
  CHECK(pages && !pages->empty(), "whole planned parts receive spatial page owners before meshing");
  if (!pages) { return; }
  const auto repeated = Corridors::PlannedPages(plan, 64, 2);
  CHECK(repeated && *repeated == *pages, "page ownership is deterministic on the immutable plan");
  std::vector<RoadPagePart> flattened;
  for (const auto &page : *pages) {
    CHECK(!page.empty() && page.size() <= 2, "dense cells keep the caller's per-page part bound");
    flattened.insert(flattened.end(), page.begin(), page.end());
    const auto recipe = Corridors::PageRecipe(plan, site, page);
    CHECK(recipe, "each spatial page produces an independently readable original recipe");
    if (!recipe) { continue; }
    std::optional<std::array<int64_t, 2>> cell;
    const auto accept = [&](const auto &points) {
      const auto key = CellOf(points);
      if (!cell) { cell = key; }
      CHECK(*cell == key, "spatial groups never combine parts assigned to different cells");
    };
    for (const auto &interval : recipe->Intervals) { accept(interval.Stations); }
    for (const auto &junction : recipe->Junctions) {
      if (!junction.Gates.empty()) { accept(junction.Gates); }
    }
  }
  std::ranges::sort(flattened, [](const auto &left, const auto &right) {
    return left.Kind != right.Kind ? left.Kind < right.Kind : left.Index < right.Index;
  });
  CHECK(
      std::ranges::equal(flattened, original),
      "spatial pages preserve every whole original part exactly once without invented identities");
  CHECK(!Corridors::PlannedPages(plan, 0, 2) && !Corridors::PlannedPages(plan, 64, 0) &&
            !Corridors::PlannedPages(plan, std::numeric_limits<double>::infinity(), 2) &&
            !Corridors::PlannedPages(plan, std::numeric_limits<double>::min(), 2),
        "invalid widths, limits and overflowing signed cells are explicit errors");
}

void CheckCancellation(const RoadPageRecipe &recipe) {
  using namespace outshine::Test;
  std::stop_source before;
  (void)before.request_stop();
  CountedMesher untouched;
  CHECK(!MeshRoadPage(untouched, recipe, before.get_token()) && untouched.Sweeps == 0 &&
            untouched.Junctions == 0,
        "an already canceled page does no mesh work");
  std::stop_source during;
  CountedMesher interrupted;
  interrupted.CancelAfterSweep = &during;
  CHECK(!MeshRoadPage(interrupted, recipe, during.get_token()) && interrupted.Sweeps == 1 &&
            interrupted.Junctions == 0,
        "cancellation discards a partial page before remaining work");
}
}

int main() {
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  const std::array<std::string, 1> layers{"streets"};
  Osm::OsmField vectors(14, layers);
  const std::array<Osm::OsmField::Declared, 3> roads{
      {{.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {48.9998, 7.9990, 49.0, 8.0, 49.0002, 8.0010}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .Bridge = true,
        .Level = 1,
        .LatLon = {48.9990, 8.0, 49.0, 8.0, 49.0010, 8.0}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {49.0, 8.0, 49.0007, 8.0008}}}};
  CHECK(vectors.Declare(roads, origin).has_value(), "source roads are declared");
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "the page uses production transport recipes");
  if (!vegetation.Ready()) { return Report(); }
  Osm::StreetField ways;
  while (!ways.Ingested(vectors)) { (void)ways.Ingest(vectors, vegetation); }
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh surface = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = surface,
                    .Field = [](Drape::EastNorth) -> std::optional<double> { return 0; }};
  const std::shared_ptr<const TerrainClassificationMap> classes;
  const Corridors::Site site{.Vectors = &vectors,
                             .Ways = ways,
                             .Materials = materials,
                             .Vegetation = vegetation,
                             .Standing = frame,
                             .Draped = drape,
                             .Classes = classes,
                             .WaterUpM = {},
                             .CensusAt = {},
                             .EyeLatDeg = origin.LatitudeDeg,
                             .EyeLonDeg = origin.LongitudeDeg,
                             .Projection = {.FocalPx = 800}};
  CountedMesher observer;
  const Corridors corridors(observer);
  auto early = Corridors::Begin(site);
  CHECK(!Corridors::PlannedParts(*early) && !Corridors::PlannedPages(*early, 64, 2) &&
            !Corridors::PageRecipe(*early, site, {}) && !Corridors::TakeTerrainContacts(*early),
        "unfinished plans cannot be used as finished assets");
  auto plan = Plan(corridors, site);
  CHECK(plan && observer.Sweeps == 0 && observer.Junctions == 0,
        "native plan completion performs no fine mesh generation");
  if (!plan) { return Report(); }
  const auto parts = Corridors::PlannedParts(*plan);
  CHECK(parts && !parts->empty(), "the completed plan exposes its actual semantic parts");
  if (!parts || parts->empty()) { return Report(); }
  Osm::OsmField foreign(14, layers);
  CHECK(foreign.Declare(roads, origin).has_value(), "a separate source has equal local ordinals");
  auto wrongSource = site;
  wrongSource.Vectors = &foreign;
  CHECK(!corridors.AdvancePlan(*plan, wrongSource, 1, 1) &&
            !Corridors::PageRecipe(*plan, wrongSource, *parts),
        "equal generation numbers cannot alias a different source context");
  CheckLayout(*plan, site, *parts);
  CHECK(observer.Sweeps == 0 && observer.Junctions == 0,
        "spatial page planning does not produce or inspect fine geometry");
  CheckSelection(*plan, site, *parts);
  const auto recipe = Corridors::PageRecipe(*plan, site, *parts);
  CHECK(recipe && !recipe->Intervals.empty(), "all requested plan data is owned by the recipe");
  if (!recipe || recipe->Intervals.empty()) { return Report(); }
  CheckCancellation(*recipe);
  ProfiledRoadMesher native;
  const Corridors reference(native);
  Geometry whole;
  std::vector<EarthworkStamp> earthworks;
  std::vector<DiagnosticSample> notes;
  CHECK(reference.Lay(site, whole, earthworks, notes),
        "the normal native path builds its reference");
  const auto contacts = Corridors::TakeTerrainContacts(*plan);
  CHECK(contacts && !contacts->empty() && *contacts == earthworks && observer.Sweeps == 0 &&
            observer.Junctions == 0,
        "complete terrain contacts precede every fine sweep and match the ordinary path");
  const auto again = Corridors::TakeTerrainContacts(*plan);
  CHECK(again && again->empty(), "terrain contact ownership transfers once without copying");
  Geometry resumed;
  std::vector<EarthworkStamp> remaining;
  std::vector<DiagnosticSample> resumedNotes;
  bool completed = false;
  for (size_t step = 0; step < 2048; ++step) {
    const auto advanced = corridors.Advance(*plan, site, 1, 1, resumed, remaining, resumedNotes);
    if (!advanced) { break; }
    if (*advanced) {
      completed = true;
      break;
    }
  }
  CHECK(completed && remaining.empty(),
        "normal meshing after contact transfer does not rebuild or duplicate terrain contacts");
  CHECK(!Corridors::TakeTerrainContacts(*plan),
        "finished meshing cannot expose an earlier plan-only ownership transfer");
  const auto mesh = MeshRoadPage(native, *recipe);
  int street = -1;
  for (int at = 0; at < whole.parts(); ++at) {
    if (whole.nameOf(at) == "streets") { street = at; }
  }
  CHECK(mesh && street >= 0 && SameMesh(mesh->Mesh, whole, street) &&
            SameMesh(mesh->Mesh, resumed, street),
        "all page words match original positions, normals, colours and primitive order");
  auto changed = roads;
  changed[0].LatLon.back() += 0.001;
  CHECK(vectors.Declare(changed, origin).has_value(),
        "source coordinates publish a new generation");
  CHECK(!corridors.AdvancePlan(*plan, site, 1, 1) && !Corridors::PageRecipe(*plan, site, *parts),
        "a changed source cannot be combined with an older plan");
  (void)plan->RetireStep(1);
  CHECK(!Corridors::PlannedParts(*plan) && !Corridors::PlannedPages(*plan, 64, 2),
        "retired plan storage is not a readable page input");
  plan.reset();
  const auto after = MeshRoadPage(native, *recipe);
  CHECK(after && street >= 0 && SameMesh(after->Mesh, whole, street),
        "page recipes retain exact native inputs after the original plan is released");
  return Report();
}

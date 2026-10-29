#include "BuildingSourceView.h"
#include "BuildingSurfaceBlock.h"
#include "BuildingScratch.h"
#include "BuildingMeshConversion.h"
#include "BuildingMaterials.h"
#include "PreparedStructureCodec.h"
#include "HeightField.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

struct Fixture {
  std::shared_ptr<PreparedStructureTile> Base;
  std::shared_ptr<size_t> Reads;
  BuildingCaptureFrame Frame;
};

enum class Availability { Ready, Missing };

std::optional<Fixture> Make(Availability availability, std::optional<Vec3f> colour = std::nullopt) {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
  raw.LatLon = {48.001, 16.001, 48.001, 16.0012, 48.0012, 16.0012, 48.0012, 16.001};
  raw.Structures.push_back({.LocalFirst = 0,
                            .PointCount = 4,
                            .SourceFirst = 0,
                            .Cell = {.Index = 1, .Footprint = {}},
                            .HeightM = 30,
                            .Pitched = 0,
                            .WallColour = colour,
                            .Facade = {}});
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "constant source heights exist");
  if (!heights) { return std::nullopt; }
  auto prepared = PrepareStructureTile(raw, *heights);
  CHECK(prepared.has_value(), "native source forms prepare");
  if (!prepared) { return std::nullopt; }
  const auto original = std::make_shared<PreparedStructureTile>(std::move(*prepared));
  const auto bytes = EncodePreparedStructureIndex(*original);
  CHECK(bytes.has_value(), "the source-only headers encode");
  if (!bytes) { return std::nullopt; }
  auto decoded = DecodePreparedStructureIndex(*bytes, size_t{4} * 1024 * 1024);
  CHECK(decoded && decoded->Surfaces.size() == 1, "headers decode independently of source forms");
  if (!decoded || decoded->Surfaces.size() != 1) { return std::nullopt; }
  const auto reads = std::make_shared<size_t>(0);
  decoded->Surfaces[0].BindShapes(
      std::make_shared<BuildingSurfaceBlock>(
          [original,
           reads,
           availability] -> std::expected<std::vector<BuildingSurface>, StructureMeshError> {
            ++*reads;
            if (availability == Availability::Missing) {
              return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
            }
            return original->Surfaces;
          }),
      0);
  const BuildingCaptureFrame frame{.Axes = EnuAxesEcef({.LongitudeDeg = 16, .LatitudeDeg = 48}),
                                   .Origin = decoded->Surfaces[0].Bounds().Middle()};
  PreparedStructure unsupported;
  unsupported.Layout.Cell.Index = 1;
  decoded->Structures.insert(decoded->Structures.begin(), std::move(unsupported));
  decoded->Surfaces.insert(decoded->Surfaces.begin(), BuildingSurface{});
  return Fixture{.Base = std::make_shared<PreparedStructureTile>(std::move(*decoded)),
                 .Reads = reads,
                 .Frame = frame};
}

void Capture(Availability availability) {
  auto fixture = Make(availability);
  if (!fixture) { return; }
  const std::array<uint32_t, 2> cells{1, 1};
  auto view = BuildingSourceView::Open(fixture->Base, cells, fixture->Frame);
  CHECK(view && *fixture->Reads == 0, "opening a view reads no lazy forms");
  if (!view) { return; }
  CHECK(view->Sources().size() == 1 && view->Sources()[0] == 1 && view->Unsupported().size() == 1 &&
            view->Unsupported()[0] == 0,
        "supported and unresolved original ordinals remain distinct");
  auto invalid = fixture->Frame;
  invalid.Axes.Up = invalid.Axes.Up * -1.0;
  CHECK(!BuildingSourceView::Open(fixture->Base, cells, invalid),
        "a reflected source frame is rejected");
  const BuildingPlaneCapture::View camera{
      .Centre = {}, .TowardEye = {{0, 0, -1}}, .HalfExtentM = 40, .Pixels = 32};
  const std::array<Material, 3> materials{Material{}, Material{}, Material{}};
  auto refused = view->Capture(camera, {.Samples = 1023, .SourceTests = 100000}, materials);
  CHECK(!refused && refused.error().Kind == BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded &&
            *fixture->Reads == 0,
        "work admission precedes source forms and mesh allocation");
  std::stop_source stop;
  stop.request_stop();
  auto cancelled =
      view->Capture(camera, {.Samples = 1024, .SourceTests = 100000}, materials, stop.get_token());
  CHECK(!cancelled && cancelled.error().Kind == BuildingPlaneCapture::ErrorKind::Cancelled &&
            *fixture->Reads == 0,
        "cancelled capture reads no forms");
  const std::weak_ptr<const PreparedStructureTile> retained = fixture->Base;
  fixture->Base.reset();
  CHECK(!retained.expired(), "the native view retains its descriptor and lazy source ownership");
  auto result = view->Capture(camera, {.Samples = 1024, .SourceTests = 100000}, materials);
  if (availability == Availability::Missing) {
    CHECK(!result && result.error().Kind == BuildingPlaneCapture::ErrorKind::SourceUnavailable &&
              result.error().Source == 1,
          "source failures retain the original ordinal after filtering");
  } else {
    CHECK(result && result->Rays == 1024 && result->Covered > 0 && result->Mesh.parts() > 0,
          "native source planes become renderable local geometry");
    if (result) {
      for (int part = 0; part < result->Mesh.parts(); ++part) {
        CHECK(std::ranges::all_of(result->Mesh.positionsOf(part),
                                  [](float x) { return std::isfinite(x) && std::abs(x) < 100; }),
              "source coordinates transform into the declared local frame");
      }
    }
  }
  CHECK(*fixture->Reads == 1, "the selected source form loads at most once");
  view = std::unexpected(BuildingSourceView::Error{.Message = "retired"});
  CHECK(retained.expired(), "retiring the final native view releases the descriptor");
}

using Triangle = std::array<std::array<float, 16>, 3>;

std::array<float, 16> VertexAttributes(const std::array<std::span<const float>, 5> &attributes,
                                       uint32_t vertex) {
  const std::array<size_t, 5> widths{3, 3, 2, 2, 4};
  std::array<float, 16> output{};
  size_t next = 0;
  for (size_t attribute = 0; attribute < attributes.size(); ++attribute) {
    const float fallback = attribute == 4 ? 1.0f : 0.0f;
    for (size_t component = 0; component < widths[attribute]; ++component) {
      output[next++] = attributes[attribute].empty()
                           ? fallback
                           : attributes[attribute][size_t{vertex} * widths[attribute] + component];
    }
  }
  return output;
}

std::vector<Triangle> Triangles(const Geometry &geometry) {
  std::vector<Triangle> output;
  for (int part = 0; part < geometry.parts(); ++part) {
    const auto indices = geometry.trianglesOf(part);
    const std::array attributes{geometry.positionsOf(part),
                                geometry.normalsOf(part),
                                geometry.textureOf(part),
                                geometry.textureOf(part, UvSet::Uv1),
                                geometry.coloursOf(part)};

    for (size_t triangle = 0; triangle < indices.size(); triangle += 3) {
      Triangle expanded{};
      for (size_t corner = 0; corner < 3; ++corner) {
        expanded[corner] = VertexAttributes(attributes, indices[triangle + corner]);
      }
      output.push_back(expanded);
    }
  }
  std::ranges::sort(output);
  return output;
}

void Originals(Availability availability) {
  auto fixture = Make(availability);
  if (!fixture) { return; }
  const std::array<uint32_t, 1> cells{1};
  auto incomplete = BuildingSourceView::Open(fixture->Base, cells, fixture->Frame);
  CHECK(incomplete.has_value(), "the source view reports incomplete membership");
  if (!incomplete) { return; }
  const auto materials = BuildingSurfaceMaterials();
  const BuildingSourceBudget enough{
      .Samples = 1024, .Sources = 1, .Faces = 100, .TriangleAttempts = 1000};
  auto unsupported = incomplete->Originals(enough, materials);
  CHECK(!unsupported &&
            unsupported.error().Kind == BuildingPlaneCapture::ErrorKind::UnsupportedSource &&
            *fixture->Reads == 0,
        "whole originals reject an unresolved member before lazy IO");
  const auto supported = std::make_shared<PreparedStructureTile>(*fixture->Base);
  supported->Structures.erase(supported->Structures.begin());
  supported->Surfaces.erase(supported->Surfaces.begin());
  auto source = BuildingSourceView::Open(supported, cells, fixture->Frame);
  CHECK(source.has_value(), "the complete supported source set opens");
  if (!source) { return; }
  std::array lowBudgets{enough, enough, enough};
  lowBudgets[0].Sources = 0;
  lowBudgets[1].Faces = 0;
  lowBudgets[2].TriangleAttempts = 0;
  for (const auto &low : lowBudgets) {
    const auto rejected = source->Originals(low, materials);
    CHECK(!rejected &&
              rejected.error().Kind == BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded &&
              *fixture->Reads == 0,
          "source/face/triangle admission precedes lazy forms");
  }
  std::stop_source stop;
  stop.request_stop();
  const auto cancelled = source->Originals(enough, materials, stop.get_token());
  CHECK(!cancelled && cancelled.error().Kind == BuildingPlaneCapture::ErrorKind::Cancelled &&
            *fixture->Reads == 0,
        "cancelled original production performs no lazy IO");
  auto tiny = enough;
  tiny.TriangleAttempts = 1;
  const auto limited = source->Originals(tiny, materials);
  if (availability == Availability::Missing) {
    CHECK(!limited && limited.error().Kind == BuildingPlaneCapture::ErrorKind::SourceUnavailable &&
              limited.error().Source == 0 && *fixture->Reads == 1,
          "missing source reports the original ordinal without partial geometry");
    return;
  }
  CHECK(!limited && limited.error().Kind == BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded,
        "the native emitter stops at its triangle attempt limit");
  auto actual = source->Originals(enough, materials);
  CHECK(actual && actual->parts() > 0 && actual->parts() <= 3 && *fixture->Reads == 1,
        "successful originals use bounded material buckets and reuse loaded forms");
  if (!actual) { return; }
  BuildingScratch scratch;
  Raised reference;
  std::vector<BuildingSurface::Face> all;
  const auto &surface = supported->Surfaces.front();
  all.reserve(surface.FaceCount());
  for (size_t face = 0; face < surface.FaceCount(); ++face) { all.push_back(surface.FaceAt(face)); }
  CHECK(surface.MeshVisible(all, scratch, reference).has_value(),
        "independent whole-face meshing succeeds");
  Geometry expected;
  for (const auto &material : materials) {
    CHECK(expected.addSurface("reference", material).has_value(), "reference material");
  }
  const std::array parts{BuildingMeshPart{.Vertices = reference.WallCorners,
                                          .Indices = reference.WallRun,
                                          .Colours = reference.WallColours,
                                          .MaterialM = reference.WallMaterialM},
                         BuildingMeshPart{.Vertices = reference.RoofCorners,
                                          .Indices = reference.RoofRun,
                                          .Colours = {},
                                          .MaterialM = {}}};
  for (size_t at = 0; at < parts.size(); ++at) {
    if (parts[at].Vertices.empty()) { continue; }
    const int role = at == 0 && !parts[at].MaterialM.empty() ? 2 : static_cast<int>(at);
    const auto part = expected.addPart("reference", MaterialInstance(role));
    auto mesh = ConvertBuildingMesh(parts[at], fixture->Frame.Axes, fixture->Frame.Origin);
    CHECK(part && mesh && expected.adoptMesh(*part, *mesh), "reference streams convert");
  }
  CHECK(Triangles(*actual) == Triangles(expected),
        "whole original triangle topology, position, normal, UV, metres and colour are exact");
}

void MixedColours(bool paintedFirst) {
  auto plain = Make(Availability::Ready);
  auto painted = Make(Availability::Ready, Vec3f{{0.2f, 0.4f, 0.6f}});
  if (!plain || !painted) { return; }
  const auto base = std::make_shared<PreparedStructureTile>(*plain->Base);
  base->Structures.erase(base->Structures.begin());
  base->Surfaces.erase(base->Surfaces.begin());
  const auto second = painted->Base->Structures[1];
  base->Structures.push_back(second);
  base->Surfaces.push_back(painted->Base->Surfaces[1]);
  if (paintedFirst) {
    std::swap(base->Structures[0], base->Structures[1]);
    std::swap(base->Surfaces[0], base->Surfaces[1]);
  }
  const std::array<uint32_t, 1> cells{1};
  const auto source = BuildingSourceView::Open(base, cells, plain->Frame);
  if (!source) {
    CHECK(false, "mixed original headers open");
    return;
  }
  const BuildingSourceBudget budget{
      .Samples = 1024, .Sources = 2, .Faces = 100, .TriangleAttempts = 1000};
  auto tooFew = budget;
  tooFew.Sources = 1;
  CHECK(!source->Originals(tooFew, BuildingSurfaceMaterials()) && *plain->Reads == 0 &&
            *painted->Reads == 0,
        "source count refusal does not load either material stream");
  auto result = source->Originals(budget, BuildingSurfaceMaterials());
  CHECK(result && *plain->Reads == 1 && *painted->Reads == 1,
        "mixed original sources load once and share bounded buckets");
  if (!result) { return; }
  bool hasPlain = false;
  bool hasPaint = false;
  size_t vertices = 0;
  for (int part = 0; part < result->parts(); ++part) {
    const auto colours = result->coloursOf(part);
    const size_t count = result->positionsOf(part).size() / 3;
    CHECK(colours.empty() || colours.size() == count * 4,
          "optional colours cover every concatenated vertex");
    for (size_t at = 0; at < colours.size(); at += 4) {
      hasPlain = hasPlain || (colours[at] == 1 && colours[at + 1] == 1 && colours[at + 2] == 1);
      hasPaint = hasPaint || (colours[at] != 1 || colours[at + 1] != 1 || colours[at + 2] != 1);
    }
    vertices += count;
  }
  CHECK(vertices > 0 && hasPlain && hasPaint,
        "plain and painted attributes survive both source orders");
}

}

int main() {
  Capture(Availability::Ready);
  Capture(Availability::Missing);
  Originals(Availability::Ready);
  Originals(Availability::Missing);
  MixedColours(false);
  MixedColours(true);
  return Report();
}

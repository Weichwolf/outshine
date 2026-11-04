#include "GeometryPageResidency.h"
#include "SceneRenderer.h"
#include "MaterialFieldPass.h"
#include "test/harness/render/NativeMaterialField.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kTile = 7;
constexpr uint64_t kSource = 9;
constexpr GeometryPageResidency::CellSource kCells{.Key = kSource, .Occupied = 3};

std::shared_ptr<const RadianceImage> Field(SceneRenderer &renderer) {
  Require(renderer.ConfigureMaterialFields());
  auto input = MaterialFieldInput::Prepare(MaterialFieldQuad({.75f, .125f, .0625f, 1}, true),
                                           {.WidthPx = 16, .HeightPx = 16});
  CHECK(input, "the field prepares its native material source");
  if (!input) { Failed(); }
  const auto source = Require(
      RetryOffline([&] { return renderer.PrepareMaterialField(*input); }, std::chrono::seconds(5)));
  MaterialFieldView view;
  view.Camera.Kind = CameraKind::Orthographic;
  view.Camera.EyeM = {{0, 0, 2}};
  view.Camera.XMagM = view.Camera.YMagM = .5;
  view.Camera.ZNearM = .1;
  view.Camera.ZFarM = 10;
  view.Target = {.WidthPx = 16, .HeightPx = 16};
  view.Encoding = RadianceEncoding::MipmappedTexels;
  return Require(RetryOffline([&] { return renderer.RelightMaterialField(*source, view); },
                              std::chrono::seconds(5)))
      .Filtered;
}

MaterialRegistration Register(SceneRenderer &renderer,
                              const std::shared_ptr<const RadianceImage> &image = {}) {
  Geometry geometry;
  Material material;
  material.Unlit = material.DoubleSided = true;
  material.BaseColour = image ? Vec4f{{1, 1, 1, 1}} : Vec4f{{.125f, .5f, .25f, 1}};
  material.Alpha = image ? AlphaMode::Blended : AlphaMode::Opaque;
  CHECK(geometry.addSurface("page", material), "the page has a native material");
  const std::array images{MaterialRadiance{.Material = 0, .Image = image}};
  auto registered = renderer.RegisterPieceMaterials(
      std::move(geometry),
      image ? std::span<const MaterialRadiance>(images) : std::span<const MaterialRadiance>{});
  CHECK(registered, "the page retains its native material and optional radiance lease");
  if (!registered) { Failed(); }
  return *registered;
}

std::array<StoredVertex, 4> Vertices(const TangentFrame &frame, Vec2 horizontal, double depth = 2) {
  const std::array<Vec2, 4> points{Vec2{{horizontal[0], -.8}},
                                   Vec2{{horizontal[1], -.8}},
                                   Vec2{{horizontal[1], .8}},
                                   Vec2{{horizontal[0], .8}}};
  std::array<StoredVertex, 4> vertices;
  for (size_t i = 0; i < points.size(); ++i) {
    const Vec3 point =
        frame.ToEcefDirection({.EastM = points[i][0], .NorthM = depth, .UpM = points[i][1]});
    const Vec3 normal = frame.ToEcefDirection({.NorthM = -1});
    vertices[i] = StoredVertex::Of({{static_cast<float>(point[0]),
                                     static_cast<float>(point[1]),
                                     static_cast<float>(point[2])}},
                                   {{i == 1 || i == 2 ? 1.0f : 0.0f, i >= 2 ? 1.0f : 0.0f}},
                                   {{static_cast<float>(normal[0]),
                                     static_cast<float>(normal[1]),
                                     static_cast<float>(normal[2])}});
  }
  return vertices;
}

bool StageCellProduct(GeometryPageResidency &pages,
                      const TangentFrame &frame,
                      uint32_t cell,
                      LevelOfDetail detail,
                      const MaterialRegistration &material,
                      GeometryPasses passes,
                      std::string &error,
                      const MaterialRegistration *shadow = nullptr) {
  const Vec2 horizontal{{cell == 1 ? -.8 : 0, cell == 1 ? 0 : .8}};
  const auto vertices = Vertices(frame, horizontal);
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  std::array parts{GeometryPagePartView{.Tangents = {},
                                        .Verts = vertices,
                                        .Indices = indices,
                                        .Clusters = {},
                                        .Colours = {},
                                        .Uv1 = {},
                                        .Surface = PieceSurface::Registered(material),
                                        .Textured = true,
                                        .Passes = passes},
                   GeometryPagePartView{}};
  const auto shadowVertices = Vertices(frame, horizontal, 1);
  if (shadow != nullptr) {
    parts[1] = parts[0];
    parts[1].Verts = shadowVertices;
    parts[1].Surface = PieceSurface::Registered(*shadow);
    parts[1].Passes = GeometryPasses::Shadow;
  }
  return pages.StageCell(kTile,
                         cell,
                         {.Parts = std::span(parts.data(), shadow != nullptr ? 2u : 1u),
                          .Digest = cell + uint64_t{static_cast<uint8_t>(detail)} * 8,
                          .AssetKey = {},
                          .FallbackHeights = false,
                          .SelectedView = {},
                          .RequestedDetail = detail,
                          .RequestedCell = cell,
                          .OccupiedCells = uint64_t{1} << (cell - 1)},
                         frame.OriginEcef(),
                         error,
                         kSource);
}

std::vector<float> Draw(SceneRenderer &renderer) {
  CHECK(renderer.RenderFrame(), "the normal scene submits its selected world pages");
  renderer.WaitForGpu();
  std::vector<float> pixels;
  CHECK(renderer.ReadSceneLinear(pixels) == ReadState::Ready &&
            pixels.size() == size_t{32} * 32 * 4,
        "the actual main image is available");
  return pixels;
}

void Exercise() {
  SceneRenderer renderer({.Create = [](void *, Native::DeviceOptions options) {
    options.Validation = true;
    return Native::CreateDevice(options);
  }});
  const auto offline = renderer.OfflineUploads(std::chrono::seconds(5));
  const auto plan =
      ExecutionPlan::Compile({.Outputs = {Resource::SceneLinear, Resource::SceneDepth},
                              .Content = {Stage::OpaqueGeometry},
                              .Exposure = {},
                              .Display = {},
                              .Precision = {}});
  CHECK(plan, "the native world plan compiles");
  if (!plan) { return; }
  const auto initialized = renderer.Init({.WidthPx = 32, .HeightPx = 32}, *plan);
  CHECK(initialized, "the native world target opens");
  if (!initialized) { return; }
  Viewpoint camera;
  camera.Kind = CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = 1;
  camera.ZNearM = .1;
  camera.ZFarM = 10;
  const auto lens = Lens::From(camera, 32, 32);
  CHECK(lens, "the world page camera has a valid lens");
  if (!lens) { return; }
  renderer.SetCamera(camera, *lens);
  const auto frame = TangentFrame::At({});
  GeometryPageResidency pages;
  pages.BindRenderer(&renderer);
  pages.SetFrame(frame);
  const auto original = Register(renderer);
  const auto vertices = Vertices(frame, {{-.8, .8}});
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  const std::array parts{GeometryPagePartView{.Tangents = {},
                                              .Verts = vertices,
                                              .Indices = indices,
                                              .Clusters = {},
                                              .Colours = {},
                                              .Uv1 = {},
                                              .Surface = PieceSurface::Registered(original),
                                              .Textured = true,
                                              .Passes = GeometryPasses::MainAndShadow}};
  std::string error;
  CHECK(pages.Upload(kTile,
                     {.Parts = parts,
                      .Digest = 10,
                      .AssetKey = {},
                      .FallbackHeights = false,
                      .SelectedView = {},
                      .RequestedDetail = LevelOfDetail::Fine,
                      .RequestedCell = {},
                      .OccupiedCells = kCells.Occupied},
                     frame.OriginEcef(),
                     error,
                     kSource),
        "the original complete tile is visible");
  const auto before = Draw(renderer);
  auto image = Field(renderer);
  const std::weak_ptr<const RadianceImage> leased = image;
  auto field = Register(renderer, image);
  CHECK(StageCellProduct(
            pages, frame, 1, LevelOfDetail::Massed, field, GeometryPasses::Main, error, &original),
        "one main-only radiance cell is staged without replacing its parent");
  field = {};
  image.reset();
  const std::array selected{
      GeometryPageResidency::CellSelection{.Cell = 1, .Detail = LevelOfDetail::Massed},
      GeometryPageResidency::CellSelection{.Cell = 2, .Detail = LevelOfDetail::Massed}};
  CHECK(!pages.ActivateCells(kTile, kCells, selected, error) && Draw(renderer) == before,
        "missing child coverage preserves the complete original image");
  CHECK(StageCellProduct(
            pages, frame, 2, LevelOfDetail::Massed, original, GeometryPasses::MainAndShadow, error),
        "the second source cell becomes resident");
  auto other = kCells;
  ++other.Key;
  CHECK(!pages.ActivateCells(kTile, other, selected, error) && Draw(renderer) == before,
        "a complete cohort from a different source revision cannot replace the original");
  CHECK(pages.ActivateCells(kTile, kCells, selected, error),
        "the complete source cohort switches atomically to its declared passes");
  const auto after = Draw(renderer);
  constexpr size_t left = (size_t{16} * 32 + 8) * 4;
  constexpr size_t right = (size_t{16} * 32 + 24) * 4;
  CHECK(after[left] == .75f && after[left + 1] == .125f && after[right] == before[right] &&
            after[right + 1] == before[right + 1],
        "the main radiance and original remainder show through the nearer shadow-only part");
  CHECK(!leased.expired() && pages.CellsActive(kTile, selected, kSource),
        "the published field holds its GPU image without its producer handles");
  CHECK(
      StageCellProduct(
          pages, frame, 1, LevelOfDetail::Fine, original, GeometryPasses::MainAndShadow, error) &&
          StageCellProduct(
              pages, frame, 2, LevelOfDetail::Fine, original, GeometryPasses::MainAndShadow, error),
      "both original fine cells become available for a view fallback");
  auto fine = selected;
  for (auto &cell : fine) { cell.Detail = LevelOfDetail::Fine; }
  CHECK(pages.ActivateCells(kTile, kCells, fine, error),
        "the complete original source is restored");
  const auto restored = Draw(renderer);
  CHECK(restored[left] == before[left] && restored[right + 1] == before[right + 1],
        "a field fallback restores the original visible surfaces");
  pages.Clear();
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native world pages require video");
  Exercise();
  SDL_Quit();
  return Report();
}

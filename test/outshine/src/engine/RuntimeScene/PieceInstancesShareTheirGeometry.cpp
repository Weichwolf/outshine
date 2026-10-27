#include "test/harness/render/SrgbColour.h"
#include <cstddef>
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <type_traits>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>
#include <SDL3/SDL.h>
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <string>
#include <string_view>
#include "ImageCapture.h"
#include "Check.h"

static_assert(!std::is_convertible_v<uint32_t, outshine::Render::PieceSurface>);
static_assert(outshine::Render::PieceSurface(7).From ==
              outshine::Render::PieceSurface::Source::Geometry);
static_assert(outshine::Render::PieceSurface::Registered(7).From ==
              outshine::Render::PieceSurface::Source::Registered);
static_assert(outshine::Render::PieceSurface::Registered(7).Index == 7);

namespace {
bool CaptureImage(outshine::Render::SceneRenderer &renderer,
                  std::string_view path,
                  std::string &error) {
  constexpr outshine::Extent frame{.WidthPx = 320, .HeightPx = 320};
  const auto saved = outshine::App::SaveImage(renderer, frame, path);
  if (!saved) { error = saved.error(); }
  return saved.has_value();
}

using namespace outshine;
using namespace outshine::Test;

Geometry MakeBaseGeometry() {
  Geometry base;
  const int part = base.addPart("offscreen", base.addSurface("white", Material{}).value()).value();
  CHECK(base.setPositions(part,
                          std::array<float, 18>{-100.4f,
                                                -0.4f,
                                                0,
                                                -99.6f,
                                                -0.4f,
                                                0,
                                                -100,
                                                0.4f,
                                                0,
                                                99.6f,
                                                -0.4f,
                                                0,
                                                100.4f,
                                                -0.4f,
                                                0,
                                                100,
                                                0.4f,
                                                0}),
        "base positions");
  CHECK(base.setNormals(
            part, std::array<float, 18>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "base normals");
  CHECK(base.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 3, 4, 5}), "base indices");
  Material masked;
  masked.BaseColour = {{1, 1, 1, 1}};
  masked.Alpha = AlphaMode::Masked;
  masked.DoubleSided = true;
  masked.BaseColourMap.Image =
      *base.addImage(2, 1, std::array<uint8_t, 8>{255, 255, 255, 0, 255, 255, 255, 255});
  masked.BaseColourMap.Sampler.Minify = masked.BaseColourMap.Sampler.Magnify = Filter::Nearest;
  masked.BaseColourMap.Sampler.Mip = MipFilter::None;
  masked.NormalMap.Image = *base.addImage(1, 1, std::array<uint8_t, 4>{191, 159, 231, 255});
  const int maskPart =
      base.addPart("masked-offscreen", base.addSurface("masked", masked).value()).value();
  CHECK(base.setPositions(maskPart,
                          std::array<float, 9>{99.6f, -0.4f, 0, 100.4f, -0.4f, 0, 100, 0.4f, 0}),
        "masked base positions");
  CHECK(base.setNormals(maskPart, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "masked base normals");
  CHECK(base.setTexture(maskPart, std::array<float, 6>{0, 0, 1, 0, 0, 1}), "masked base UVs");
  CHECK(base.setTriangles(maskPart, std::array<uint32_t, 3>{0, 1, 2}), "masked base indices");
  return base;
}

struct PieceFixture {
  Geometry base = MakeBaseGeometry();
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  Render::Viewpoint eye;
  const std::array<StoredVertex, 3> vertices{
      {StoredVertex::Of({{-0.4f, -0.4f, 0}}, {{0, 0}}, {{0, 0, 1}}),
       StoredVertex::Of({{0.4f, -0.4f, 0}}, {{0, 0}}, {{0, 0, 1}}),
       StoredVertex::Of({{0, 0.4f, 0}}, {{0, 0}}, {{0, 0, 1}})}};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  std::array<Mat4, 3> rows{};
  const std::array<uint32_t, 2> surfaces{0, 1};
  const std::array<uint8_t, 4> green{32, 180, 64, 255};

  bool Open() {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 320;
    declaration.InitialGeometry = &base;
    declaration.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
    declaration.KeyLux = 20000;
    declaration.Exposure = 1.0;
    declaration.KeyElevationDeg = 45;
    declaration.KeyBearingDeg = 180;
    if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) {
      Unprepared(error.c_str());
      return false;
    }
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    eye.ZNearM = 0.1;
    eye.ZFarM = 10;
    scene->Eye(eye);
    rows[0][12] = 100;
    rows[1][12] = -1;
    rows[2][12] = 1;
    renderer.SetNativePieceSurfaces(surfaces);
    std::filesystem::create_directories("build/instance-native");
    return true;
  }

  bool Screenshot(std::string_view path, std::string &why) {
    return CaptureImage(renderer, path, why);
  }

  void CheckPlacements(bool clustered) {
    const std::array<DagCluster, 1> clusters{
        {{.SelfRadius = 0.6f, .ParentRadius = 0.6f, .ParentErr = kDagRootErr, .Count = 3}}};
    Render::PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = indices;
    piece.Instances = rows;
    piece.MaxInstances = rows.size();
    if (clustered) { piece.Clusters = clusters; }
    const auto id = renderer.PlacePiece(piece, error);
    CHECK(id != Render::kNoPiece, "three placements accept one prototype");
    const std::array<Mat4, 4> tooMany{};
    CHECK(!renderer.SetPieceInstances(id, tooMany, error),
          "instance updates refuse capacity overflow without replacing rows");
    CHECK(!renderer.SetPieceInstances(Render::kNoPiece, rows, error),
          "instance updates refuse an unknown piece");
    error.clear();
    CHECK(renderer.PiecesStanding() == 1 && renderer.PieceTriangles() == 1,
          "resident triangle count does not multiply with placements");
    Render::PieceMesh neighbour;
    neighbour.Verts = vertices;
    neighbour.Indices = indices;
    neighbour.Row[13] = 1;
    const auto neighbourId = renderer.PlacePiece(neighbour, error);
    CHECK(neighbourId != Render::kNoPiece, "a following single-placement mesh accepts its own row");
    CHECK(scene->Draw(error), "instanced frame draws");
    renderer.WaitForGpu();
    std::vector<float> depth;
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready, "depth is read");
    CHECK(depth.size() == size_t{320} * 320, "complete depth frame");
    if (depth.size() == size_t{320} * 320) {
      CHECK(depth[size_t{160} * 320 + 80u] > 0 && depth[size_t{160} * 320 + 240u] > 0,
            "both visible instances survive even when the first is outside the frustum");
      CHECK(depth[size_t{160} * 320 + 160u] == 0, "the gap contains no unplaced prototype");
    }
    CHECK(Screenshot(clustered ? "build/instance-native/clustered.png"
                               : "build/instance-native/instanced.png",
                     error),
          "instanced PNG is written");
    const auto placedDepth = depth;
    CHECK(renderer.SetPieceInstances(id, {}, error) && scene->Draw(error),
          "empty instance update deactivates the prototype");
    renderer.WaitForGpu();
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready &&
              depth.size() == size_t{320} * 320 && depth[size_t{160} * 320 + 80u] == 0 &&
              depth[size_t{160} * 320 + 240u] == 0 && depth[size_t{80} * 320 + 160u] > 0,
          "empty rows remove both instances while the following piece keeps its placement");
    CHECK(renderer.SetPieceInstances(id, rows, error) && scene->Draw(error),
          "resident prototype accepts its instances again");
    renderer.WaitForGpu();
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready && depth == placedDepth,
          "restored instance rows reproduce the complete depth frame");
    renderer.ReleasePiece(id);
    CHECK(!renderer.SetPieceInstances(id, rows, error), "instance updates refuse a released piece");
    error.clear();
    CHECK(renderer.PiecesStanding() == 1 && renderer.PieceTriangles() == 1,
          "release retains only the other prototype");
    CHECK(scene->Draw(error), "released frame draws");
    renderer.WaitForGpu();
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready, "released depth is read");
    CHECK(depth.size() == size_t{320} * 320 && depth[size_t{160} * 320 + 80u] == 0 &&
              depth[size_t{160} * 320 + 240u] == 0,
          "release removes every instance");
    CHECK(depth.size() == size_t{320} * 320 && depth[size_t{80} * 320 + 160u] > 0,
          "the following piece keeps its matrix after earlier rows are released");
    renderer.ReleasePiece(neighbourId);
    CHECK(renderer.PiecesStanding() == 0 && renderer.PieceTriangles() == 0,
          "both allocations are released without counter underflow");
  }

  void CheckMaskedInstances(bool clustered, bool back) {
    const std::array<StoredVertex, 4> card{
        {StoredVertex::Of({{-0.4f, -0.4f, 0}}, {{0, 1}}, {{0, 0, 1}}),
         StoredVertex::Of({{0.4f, -0.4f, 0}}, {{1, 1}}, {{0, 0, 1}}),
         StoredVertex::Of({{0.4f, 0.4f, 0}}, {{1, 0}}, {{0, 0, 1}}),
         StoredVertex::Of({{-0.4f, 0.4f, 0}}, {{0, 0}}, {{0, 0, 1}})}};
    const std::array<float, 16> tangents{1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1};
    const std::array<uint32_t, 6> cardIndices{0, 1, 2, 0, 2, 3};
    const std::array<DagCluster, 1> cardClusters{
        {{.SelfRadius = 0.6f, .ParentRadius = 0.6f, .ParentErr = kDagRootErr, .Count = 6}}};
    std::array<Mat4, 2> placements{rows[1], rows[2]};
    if (back) {
      for (auto &placement : placements) { placement[0] = placement[10] = -1; }
    }
    Render::PieceMesh piece;
    piece.Verts = card;
    piece.Tangents = tangents;
    piece.Indices = cardIndices;
    piece.Instances = placements;
    piece.Surface = Render::PieceSurface(1);
    piece.Textured = true;
    if (clustered) { piece.Clusters = cardClusters; }
    auto invalid = piece;
    invalid.Tangents = std::span<const float>(tangents).first(12);
    CHECK(renderer.PlacePiece(invalid, error) == Render::kNoPiece,
          "incomplete tangent streams are refused");
    invalid = piece;
    invalid.Textured = false;
    CHECK(renderer.PlacePiece(invalid, error) == Render::kNoPiece,
          "tangents without UVs are refused");
    const auto id = renderer.PlacePiece(piece, error);
    CHECK(id != Render::kNoPiece && scene->Draw(error), "masked instanced cards draw");
    renderer.WaitForGpu();
    std::vector<float> depth;
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready &&
              depth.size() == size_t{320} * 320,
          "masked card depth is complete");
    if (depth.size() == size_t{320} * 320) {
      for (const size_t centre : {80u, 240u}) {
        const size_t opaque = back ? centre - 20u : centre + 20u;
        const size_t clear = back ? centre + 20u : centre - 20u;
        CHECK(depth[size_t{160} * 320 + opaque] > 0,
              "the opaque half survives on either side of each instance");
        CHECK(depth[size_t{160} * 320 + clear] == 0,
              "the masked half writes no depth on either side of each instance");
      }
    }
    std::vector<float> normals;
    CHECK(renderer.ReadShadingNormal(normals) == Render::ReadState::Ready &&
              normals.size() == size_t{320} * 320 * 4u,
          "instanced shading normals are readable");
    Vec3 expected{{191.0 / 127.5 - 1, -(159.0 / 127.5 - 1), 231.0 / 127.5 - 1}};
    if (back) { expected[1] = -expected[1]; }
    CHECK(Normalise(expected), "the external normal-map direction is nonzero");
    if (normals.size() == size_t{320} * 320 * 4u) {
      for (const size_t centre : {80u, 240u}) {
        const size_t opaque = back ? centre - 20u : centre + 20u;
        const size_t pixel = (size_t{160} * 320 + opaque) * 4u;
        const Vec3 actual{{normals[pixel], normals[pixel + 1], normals[pixel + 2]}};
        const auto delta = actual - expected;
        CHECK(std::sqrt(Dot(delta, delta)) < std::sqrt(3.0) / 1024,
              "rotated double-sided instances preserve the normal-map world direction");
      }
    }
    const std::string path = std::string("build/instance-native/masked-") +
                             (clustered ? "clustered-" : "direct-") +
                             (back ? "back.png" : "front.png");
    CHECK(Screenshot(path, error), "masked card PNG is written");
    renderer.ReleasePiece(id);
  }

  void CheckMaterialArrival() {
    Render::PieceMesh resident;
    resident.Verts = vertices;
    resident.Indices = indices;
    resident.Row = rows[1];
    const auto oldPiece = renderer.PlacePiece(resident, error);
    CHECK(oldPiece != Render::kNoPiece && scene->Draw(error),
          "resident prototype stands before material arrival");
    renderer.WaitForGpu();
    std::vector<float> before;
    std::vector<float> after;
    CHECK(renderer.ReadSceneLinear(before) == Render::ReadState::Ready &&
              before.size() == size_t{320} * 320 * 4u,
          "resident colour is captured before arrival");
    CHECK(Screenshot("build/instance-native/material-before.png", error),
          "before-arrival PNG is written");
    std::array<Render::MaterialBinding, 2> rejected;
    rejected[1].Row.Transmission = 1;
    CHECK(!renderer.AppendGeometryMaterials(rejected, error),
          "unsupported second material refuses the complete append");
    error.clear();
    Render::MaterialBinding arriving;
    arriving.Row.Unlit = true;
    arriving.Row.BaseColour = {{0.5f, 1.0f, 0.5f, 1.0f}};
    arriving.Colour.Rgba = green.data();
    arriving.Colour.Width = arriving.Colour.Height = 1;
    CHECK(renderer.AppendGeometryMaterials(std::span(&arriving, 1), error),
          "a texture material appends to the resident table");
    CHECK(scene->Draw(error), "resident draws survive material arrival");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(after) == Render::ReadState::Ready && after == before,
          "appending a surface preserves every resident pixel");
    const std::array<uint32_t, 3> extendedSurfaces{0, 1, 2};
    renderer.SetNativePieceSurfaces(extendedSurfaces);
    resident.Row = rows[2];
    resident.Surface = Render::PieceSurface(2);
    resident.Textured = true;
    const auto newPiece = renderer.PlacePiece(resident, error);
    CHECK(newPiece != Render::kNoPiece && scene->Draw(error),
          "a new prototype draws with the appended surface");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(after) == Render::ReadState::Ready &&
              after.size() == before.size(),
          "both resident and arriving prototype colours are readable");
    if (after.size() == size_t{320} * 320 * 4u && before.size() == after.size()) {
      const size_t oldPixel = (size_t{160} * 320 + 80u) * 4u;
      const size_t newPixel = (size_t{160} * 320 + 240u) * 4u;
      CHECK(std::equal(
                before.begin() + oldPixel, before.begin() + oldPixel + 4, after.begin() + oldPixel),
            "old material slot remains unchanged after new geometry arrives");
      CHECK(after[newPixel + 1] > 0.4f && after[newPixel] < 0.1f && after[newPixel + 2] < 0.1f,
            "the first appended slot samples its green texture without a partial rejected batch");
      for (size_t channel = 0; channel < 3; ++channel) {
        CHECK(EncodeSrgb8(after[newPixel + channel] / arriving.Row.BaseColour[channel]) ==
                  green[channel],
              "unlit instance preserves each source colour code after its material factor");
      }
    }
    CHECK(Screenshot("build/instance-native/material-after.png", error),
          "after-arrival PNG is written");
    renderer.ReleasePiece(newPiece);
    renderer.ReleasePiece(oldPiece);
    CHECK(
        Core::RuntimeScene::ReplacesGeometry(renderer, *scene, base.clone(), nullptr, scene, error),
        "native geometry replacement publishes around the direct renderer registration fixture");
    CHECK(scene->DrivenParts() == static_cast<size_t>(base.parts()),
          "a complete native replacement drives every replacement part");
    scene->Eye(eye);
  }

  void CheckRejectedRegistrations() {
    Geometry empty;
    CHECK(!renderer.RegisterPieceMaterials(std::move(empty)),
          "registration refuses an empty material source");
    Geometry missing;
    Material absent;
    absent.BaseColourMap.Image = 0;
    (void)missing.addSurface("missing image", absent).value();
    CHECK(!renderer.RegisterPieceMaterials(std::move(missing)),
          "registration refuses a missing native image");
    Geometry glass;
    Material transmitted;
    transmitted.Transmission = 1;
    (void)glass.addSurface("undeclared glass pass", transmitted).value();
    CHECK(!renderer.RegisterPieceMaterials(std::move(glass)),
          "registration refuses an unsupported pass");
    error.clear();
  }

  auto RegisterColour(const std::array<uint8_t, 4> &colour) {
    Geometry source;
    Material material;
    material.BaseColour = {{1, 1, 1, 1}};
    material.Unlit = true;
    material.BaseColourMap.Image = *source.addImage(1, 1, colour);
    (void)source.addSurface("registered prototype", material).value();
    return renderer.RegisterPieceMaterials(std::move(source));
  }

  bool CheckRegisteredMaterials() {
    const auto registeredGreen = RegisterColour(green);
    CHECK(registeredGreen && *registeredGreen == 0,
          "refused registrations consume no stable material handles");
    if (!registeredGreen) { return false; }
    Render::PieceMesh owned;
    owned.Verts = vertices;
    owned.Indices = indices;
    owned.Instances = std::span(rows).subspan(1);
    owned.Surface = Render::PieceSurface::Registered(*registeredGreen);
    owned.Textured = true;
    const auto ownedPiece = renderer.PlacePiece(owned);
    CHECK(ownedPiece.has_value() && scene->Draw(error),
          "registered texture reaches two instances after source destruction");
    renderer.WaitForGpu();
    std::vector<float> registeredBefore;
    std::vector<float> registeredAfter;
    CHECK(renderer.ReadSceneLinear(registeredBefore) == Render::ReadState::Ready &&
              registeredBefore.size() == size_t{320} * 320 * 4u,
          "registered instances have a complete linear image");
    if (registeredBefore.size() == size_t{320} * 320 * 4u) {
      for (const size_t x : {80u, 240u}) {
        for (size_t channel = 0; channel < 3; ++channel) {
          CHECK(EncodeSrgb8(registeredBefore[(size_t{160} * 320 + x) * 4u + channel]) ==
                    green[channel],
                "RuntimeScene-owned prototype images preserve each source RGB code");
        }
      }
    }
    const auto registeredBlue = RegisterColour({32, 64, 180, 255});
    CHECK(registeredBlue && *registeredBlue == *registeredGreen + 1,
          "another prototype receives a distinct stable handle");
    CHECK(scene->Draw(error), "new registration preserves the existing instances");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(registeredAfter) == Render::ReadState::Ready &&
              registeredAfter == registeredBefore,
          "another image owner does not alter resident pixels");
    CHECK(Screenshot("build/instance-native/registered-before.png", error),
          "registered before-rebuild PNG is written");
    (void)base.addSurface("extra native material one", Material{}).value();
    (void)base.addSurface("extra native material two", Material{}).value();
    CHECK(
        Core::RuntimeScene::ReplacesGeometry(renderer, *scene, base.clone(), nullptr, scene, error),
        "native material growth publishes around resident registered pieces");
    scene->Eye(eye);
    CHECK(scene->Draw(error), "registered instances draw after native material indices shift");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(registeredAfter) == Render::ReadState::Ready &&
              registeredAfter == registeredBefore,
          "rebuilding the native surface table preserves every registered pixel without replacing "
          "pieces");
    CHECK(Screenshot("build/instance-native/registered-after.png", error),
          "registered after-rebuild PNG is written");
    if (registeredBlue && !CheckSecondRegistration(owned, *registeredBlue, registeredAfter)) {
      return false;
    }
    if (!ownedPiece) { return false; }
    renderer.ReleasePiece(*ownedPiece);
    return true;
  }

  bool CheckSecondRegistration(Render::PieceMesh &owned,
                               uint32_t registeredBlue,
                               std::vector<float> &registeredAfter) {
    owned.Instances = {};
    owned.Row[13] = 1;
    owned.Surface = Render::PieceSurface::Registered(registeredBlue);
    const auto bluePiece = renderer.PlacePiece(owned);
    CHECK(bluePiece.has_value() && scene->Draw(error),
          "a retained second handle remains usable after rebuild");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(registeredAfter) == Render::ReadState::Ready &&
              registeredAfter.size() == size_t{320} * 320 * 4u,
          "second retained prototype has readable pixels");
    if (registeredAfter.size() == size_t{320} * 320 * 4u) {
      const size_t pixel = (size_t{80} * 320 + 160u) * 4u;
      CHECK(registeredAfter[pixel + 2] > 0.4f && registeredAfter[pixel] < 0.1f &&
                registeredAfter[pixel + 1] < 0.1f,
            "the second handle still samples its blue image");
    }
    CHECK(Screenshot("build/instance-native/registered-both.png", error),
          "both retained prototype PNG is written");
    if (!bluePiece) { return false; }
    renderer.ReleasePiece(*bluePiece);
    CHECK(!renderer.SetPieceInstances(*bluePiece, {}, error),
          "a released RuntimeScene handle cannot address a future renderer piece");
    return true;
  }
};
}

int main() {
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  PieceFixture fixture;
  if (!fixture.Open()) { return Report(); }
  for (const bool clustered : {false, true}) { fixture.CheckPlacements(clustered); }
  for (const bool clustered : {false, true}) {
    for (const bool back : {false, true}) { fixture.CheckMaskedInstances(clustered, back); }
  }
  fixture.CheckMaterialArrival();
  fixture.CheckRejectedRegistrations();
  (void)fixture.CheckRegisteredMaterials();
  return Report();
}

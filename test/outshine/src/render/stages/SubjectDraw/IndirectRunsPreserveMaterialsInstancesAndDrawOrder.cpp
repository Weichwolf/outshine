#include "Check.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include "RuntimeScene.h"
#include "WorldCandidate.h"
#include "SceneRenderer.h"
#include "Viewing.h"
#include "ClusterDag.h"
#include "StoredVertex.h"
#include "SubjectTypes.h"

#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
bool Populate(outshine::Render::SceneRenderer &renderer, bool clustered, std::string &error) {
  using namespace outshine;
  using namespace outshine::Render;
  const std::array<Vec3f, 4> points{
      {{{-0.13f, -0.13f, -5}}, {{0.13f, -0.13f, -5}}, {{0.13f, 0.13f, -5}}, {{-0.13f, 0.13f, -5}}}};
  std::array<StoredVertex, 4> vertices{};
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] = StoredVertex::Of(points[at], {{0, 0}}, {{0, 0, 1}});
  }
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  const std::array<DagCluster, 1> clusters{{{.SelfCenter = {{0, 0, -5}},
                                             .SelfRadius = 0.2f,
                                             .ParentCenter = {{0, 0, -5}},
                                             .ParentRadius = 0.2f,
                                             .ParentErr = kDagRootErr,
                                             .Count = indices.size()}}};
  constexpr uint32_t columns = 6;
  constexpr uint32_t rows = 4;
  constexpr float firstWidth = 0.6f;
  constexpr float widthStep = 0.01f;
  constexpr double columnSpacing = 0.65;
  constexpr double rowSpacing = 0.9;
  constexpr double instanceRight = 0.2;
  constexpr double instanceUp = 0.3;
  for (uint32_t at = 0; at < columns * rows; ++at) {
    auto shape = vertices;
    for (auto &vertex : shape) { vertex.pos[0] *= firstWidth + static_cast<float>(at) * widthStep; }
    const Mat4 identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    std::array<Mat4, 2> instances{identity, identity};
    const uint32_t row = at / columns;
    instances[0][12] = (static_cast<double>(at % columns) - (columns - 1) * 0.5) * columnSpacing;
    instances[0][13] = (static_cast<double>(row) - (rows - 1) * 0.5) * rowSpacing;
    instances[1] = instances[0];
    instances[1][12] += instanceRight;
    instances[1][13] += instanceUp;
    PieceMesh mesh;
    mesh.Verts = shape;
    mesh.Indices = indices;
    mesh.Clusters = clustered && at != 12 ? std::span(clusters) : std::span<const DagCluster>{};
    mesh.Instances = instances;
    mesh.Surface = PieceSurface(at % 2);
    mesh.Textured = at == 14;
    const auto placed = renderer.PlacePiece(mesh);
    if (!placed) {
      error = placed.error();
      return false;
    }
  }
  return true;
}

void Compare(outshine::Render::SceneRenderer &actual, outshine::Render::SceneRenderer &oracle) {
  using namespace outshine::Render;
  using namespace outshine::Test;
  std::vector<float> pixels;
  std::vector<float> expected;
  std::vector<float> depth;
  std::vector<float> expectedDepth;
  CHECK(actual.ReadSceneLinear(pixels) == ReadState::Ready &&
            oracle.ReadSceneLinear(expected) == ReadState::Ready && pixels == expected,
        "indirect runs preserve every pixel of the independent direct-draw oracle");
  CHECK(actual.ReadDepth(depth) == ReadState::Ready &&
            oracle.ReadDepth(expectedDepth) == ReadState::Ready && depth == expectedDepth,
        "indirect runs preserve depth and draw order exactly");
  size_t red = 0;
  size_t green = 0;
  constexpr float bright = 0.9f;
  constexpr float dark = 0.1f;
  for (size_t at = 0; at + 3 < expected.size(); at += 4) {
    if (expected[at] > bright && expected[at + 1] < dark) { ++red; }
    if (expected[at + 1] > bright && expected[at] < dark) { ++green; }
  }
  CHECK(red > 100 && green > 100, "both independent materials contribute visible pixels");
  CHECK(actual.SubjectBatchCount() == 48 && oracle.SubjectBatchCount() == 25,
        "all instance batches and the direct fallback remain present");
  CHECK(actual.SubjectDrawCallCount() == 6 && oracle.SubjectDrawCallCount() == 25,
        "material, layout and direct-batch boundaries split compatible indirect runs");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for indirect draw runs");
  {
    Geometry geometry;
    Material red;
    red.Unlit = true;
    red.BaseColour = {{1, 0, 0, 1}};
    Material green = red;
    green.BaseColour = {{0, 1, 0, 1}};
    const auto first = geometry.addSurface("red", red);
    const auto second = geometry.addSurface("green", green);
    CHECK(first && second, "distinct opaque materials exist");
    if (!first || !second) { return Report(); }
    const auto part = geometry.addPart("offscreen", *first);
    CHECK(
        part &&
            geometry.setPositions(*part, std::array<float, 9>{99, 0, -5, 100, 0, -5, 99, 1, -5}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the shared initial direct batch is outside the view");
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 128;
    declaration.Outputs = {"sceneLinear"};
    declaration.Stages = {"subjects"};
    SceneRenderer actual;
    SceneRenderer oracle;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::unique_ptr<Core::RuntimeScene> control;
    std::string error;
    CHECK(Core::RuntimeScene::Open(actual, declaration, nullptr, scene, error) &&
              Core::RuntimeScene::Open(oracle, declaration, nullptr, control, error),
          "clustered and direct renderers open identical scenes");
    if (!scene || !control) { return Report(); }
    CHECK(Populate(actual, true, error) && Populate(oracle, false, error),
          "identical source quads, placements and material boundaries populate");
    Viewpoint eye;
    constexpr double nearPlaneM = 0.1;
    constexpr double cameraMoveM = 0.2;
    eye.YfovRad = 1;
    eye.ZNearM = nearPlaneM;
    eye.ZFarM = 100.0;
    scene->Eye(eye);
    control->Eye(eye);
    CHECK(scene->Draw(error) && control->Draw(error), "both draw paths submit successfully");
    Compare(actual, oracle);
    Core::WorldCandidate restored(actual);
    Core::WorldCandidate restoredOracle(oracle);
    const auto prepared = restored.Prepare(*scene, nullptr);
    const auto preparedOracle = restoredOracle.Prepare(*control, nullptr);
    CHECK(prepared && preparedOracle, "both worlds restore the same native piece sources");
    if (!prepared || !preparedOracle) { return Report(); }
    CHECK(restored.Publish(scene) && restoredOracle.Publish(control),
          "native source worlds publish after independent range allocation");
    scene->Eye(eye);
    control->Eye(eye);
    CHECK(scene->Draw(error) && control->Draw(error), "restored worlds draw successfully");
    Compare(actual, oracle);
    eye.EyeM[0] = cameraMoveM;
    scene->Eye(eye);
    control->Eye(eye);
    CHECK(scene->Draw(error) && control->Draw(error), "camera movement updates both draw paths");
    Compare(actual, oracle);
  }
  SDL_Quit();
  return Report();
}

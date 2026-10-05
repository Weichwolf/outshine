#include "Check.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include "RuntimeScene.h"
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
  for (uint32_t at = 0; at < 24; ++at) {
    const Mat4 identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    std::array<Mat4, 2> instances{identity, identity};
    instances[0][12] = (static_cast<double>(at % 6) - 2.5) * 0.65;
    instances[0][13] = (static_cast<double>(at / 6) - 1.5) * 0.9;
    instances[1] = instances[0];
    instances[1][12] += 0.2;
    instances[1][13] += 0.3;
    const PieceMesh mesh{.Verts = vertices,
                         .Indices = indices,
                         .Clusters = clustered && at != 12 ? std::span(clusters)
                                                           : std::span<const DagCluster>{},
                         .Instances = instances,
                         .Surface = PieceSurface(at % 2),
                         .Textured = at == 14};
    if (renderer.PlacePiece(mesh, error) == kNoPiece) { return false; }
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
  for (size_t at = 0; at + 3 < expected.size(); at += 4) {
    red += expected[at] > 0.9f && expected[at + 1] < 0.1f;
    green += expected[at + 1] > 0.9f && expected[at] < 0.1f;
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
    eye.YfovRad = 1;
    eye.ZNearM = 0.1;
    eye.ZFarM = 100;
    scene->Eye(eye);
    control->Eye(eye);
    CHECK(scene->Draw(error) && control->Draw(error), "both draw paths submit successfully");
    Compare(actual, oracle);
    eye.EyeM[0] = 0.2;
    scene->Eye(eye);
    control->Eye(eye);
    CHECK(scene->Draw(error) && control->Draw(error), "camera movement updates both draw paths");
    Compare(actual, oracle);
  }
  SDL_Quit();
  return Report();
}

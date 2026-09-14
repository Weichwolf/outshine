#include "Live.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video initializes");
  if (!initialized) { return Report(); }
  {
    constexpr std::array<float, 9> positions{-1, -1, 0, 1, -1, 0, 0, 1, 0};
    constexpr std::array<float, 9> emitted{};
    constexpr std::array<uint32_t, 3> indices{0, 1, 2};
    Geometry geometry;
    const int part = geometry.addPart("triangle", MaterialInstance{});
    CHECK(geometry.setPositions(part, positions) && geometry.setTriangles(part, indices),
          "native triangle prepared");
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Surfacing.front().Unlit = true;
    SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "native scene opens");
    if (scene) {
      SubjectPose pose;
      pose.Verts.From = positions.data();
      pose.Emitted.From = emitted.data();
      pose.VertexCount = 3;
      CHECK(renderer.SetSubjectPose(pose, error), "initial mesh accepts a native pose");
      CHECK(scene->Draw(error), "initial geometry renders");
      renderer.WaitForGpu();
      std::vector<float> before;
      CHECK(renderer.ReadSceneLinear(before) == ReadState::Ready && !before.empty(),
            "baseline pixels are readable");
      const auto uploads = renderer.TotalUploadAttempts();
      const auto batches = renderer.SubjectBatchCount();
      CHECK(batches > 0, "baseline contains actual draw batches");
      constexpr auto maximum = std::numeric_limits<uint32_t>::max();
      for (int fault = 0; fault < 10; ++fault) {
        DrawList draws;
        CHECK(draws.Add(DrawItem{.IndexCount = 3}, error), "valid independent draw added");
        draws.Compile();
        SubjectMesh mesh;
        mesh.Verts.From = positions.data();
        mesh.Emitted.From = emitted.data();
        mesh.VertexCount = 3;
        mesh.Indices = indices.data();
        mesh.IndexCount = 3;
        mesh.Draws = &draws;
        // Deliberately corrupt a compiled batch to test the consumer independently of DrawList.
        auto &batch = const_cast<DrawBatch &>(draws.Batches().front());
        switch (fault) {
          case 0: mesh.Verts = {}; break;
          case 1: mesh.Indices = nullptr; break;
          case 2: mesh.Draws = nullptr; break;
          case 3: mesh.Emitted = {}; break;
          case 4: batch.FirstIndex = maximum; break;
          case 5:
            batch.ModelSlot = maximum;
            batch.Instances = 2;
            break;
          case 6: batch.Layout = static_cast<VertexLayout>(255); break;
          case 7: batch.Layout = VertexLayout::PositionUv; break;
          case 8: batch.MaterialSlot = maximum; break;
          case 9: batch.FirstIndex = 1; break;
        }
        error.clear();
        CHECK(!renderer.SetSubjectMesh(mesh, error) && !error.empty(),
              "invalid mesh is rejected with a diagnosis");
        CHECK(renderer.TotalUploadAttempts() == uploads && renderer.SubjectBatchCount() == batches,
              "rejection preserves uploaded geometry and draw tables");
        CHECK(renderer.SetSubjectPose(pose, error),
              "valid pose update still finds the previous resident mesh after rejection");
        CHECK(scene->Draw(error), "old geometry remains drawable after rejection");
        renderer.WaitForGpu();
        std::vector<float> after;
        CHECK(renderer.ReadSceneLinear(after) == ReadState::Ready && after == before,
              "invalid input leaves the rendered image unchanged");
      }
    }
  }
  SDL_Quit();
  return Report();
}

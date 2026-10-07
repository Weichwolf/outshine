#include "Check.h"
#include "Heap.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

void *operator new(size_t bytes) {
  return outshine::Heap::Take("object", bytes);
}

void operator delete(void *block) noexcept {
  outshine::Heap::Return(block);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Heap::EnableProcessInstrumentation();
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (!scene) { return Report(); }
    Render::Viewpoint eye;
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    eye.ZNearM = 0.1;
    eye.ZFarM = 10;
    scene->Eye(eye);
    constexpr size_t imageBytes = 1024 * 1024 * 4;
    Geometry materials;
    {
      std::vector<uint8_t> pixels(imageBytes, 255);
      for (size_t at = 0; at < imageBytes; at += 4) {
        pixels[at] = 32;
        pixels[at + 1] = 128;
      }
      const auto image = materials.addImage(1024, 1024, pixels);
      CHECK(image, "large native image is valid");
      if (!image) { return Report(); }
      Material mapped;
      mapped.Unlit = true;
      mapped.BaseColourMap.Image = *image;
      CHECK(materials.addSurface("mapped", mapped), "mapped material is valid");
    }
    const auto registered = renderer.RegisterPieceMaterials(std::move(materials));
    CHECK(registered, "native material takes ownership of its image");
    if (!registered) { return Report(); }
    const std::array vertices{StoredVertex::Of({{-1, -1, 0}}, {{0, 0}}, {{0, 0, 1}}),
                              StoredVertex::Of({{1, -1, 0}}, {{1, 0}}, {{0, 0, 1}}),
                              StoredVertex::Of({{0, 1, 0}}, {{0.5f, 1}}, {{0, 0, 1}})};
    const std::array<uint32_t, 3> indices{0, 1, 2};
    const auto piece =
        renderer.PlacePiece({.Verts = vertices,
                             .Indices = indices,
                             .Surface = Render::PieceSurface::Registered(*registered),
                             .Textured = true});
    CHECK(piece && scene->Draw(error), "mapped piece draws before world replacement");
    renderer.WaitForGpu();
    std::vector<float> before;
    CHECK(renderer.ReadSceneLinear(before) == Render::ReadState::Ready && !before.empty(),
          "mapped pixels are readable");
    static const Heap::Tag tag("candidate-material-test");
    for (int attempt = 0; attempt < 3; ++attempt) {
      const size_t taken = Heap::TakenUnder("candidate-material-test");
      bool begun = false;
      {
        const Heap::Tagged scope(tag);
        begun = renderer.BeginsWorldCandidate(error);
      }
      const size_t bytes = Heap::TakenUnder("candidate-material-test") - taken;
      std::printf("candidate CPU allocations: %zu bytes for %zu image bytes\n", bytes, imageBytes);
      CHECK(begun, "candidate snapshots the registered resources");
      CHECK(bytes < imageBytes / 8,
            "candidate metadata cannot allocate another copy of immutable image pixels");
      renderer.AbandonsWorldCandidate();
    }
    Core::WorldCandidate candidate(renderer);
    CHECK(candidate.Prepare(*scene, nullptr), "replacement prepares with the shared images");
    CHECK(candidate.SetGeometry(Geometry{}, 0, error), "replacement uses native resources");
    CHECK(candidate.Publish(scene), "replacement releases the old world");
    scene->Eye(eye);
    CHECK(scene->Draw(error), "images remain alive after the old world is destroyed");
    renderer.WaitForGpu();
    std::vector<float> after;
    CHECK(renderer.ReadSceneLinear(after) == Render::ReadState::Ready && after == before,
          "shared image ownership preserves every rendered pixel");
  }
  SDL_Quit();
  return Report();
}

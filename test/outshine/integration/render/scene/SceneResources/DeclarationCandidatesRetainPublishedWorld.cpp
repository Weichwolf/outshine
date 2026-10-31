#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

struct DeclarationFixture {
  Render::SceneRenderer Renderer;
  Geometry Original;
  Geometry Replacement;
  Core::Declaration Declaration;
  Render::Viewpoint Eye;
  std::string Error;
  MaterialInstance Surface;
  std::unique_ptr<Core::RuntimeScene> Scene;

  bool Open() {
    Material material;
    material.Unlit = true;
    material.BaseColour = {{1, 0, 0, 1}};
    Surface = Original.addSurface("red", material).value();
    const auto part = Original.addPart("triangle", Surface).value();
    CHECK(Original.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
              Original.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "the published triangle has valid geometry");
    Replacement = Original.clone();
    Declaration.InitialGeometry = &Original;
    Declaration.SurfaceWidthPx = Declaration.SurfaceHeightPx = 32;
    Declaration.Outputs = {"sceneLinear"};
    const bool opened = Core::RuntimeScene::Open(Renderer, Declaration, nullptr, Scene, Error);
    CHECK(opened, Error.c_str());
    if (!opened) { return false; }
    Eye.EyeM = {{0, 0, 4}};
    Eye.Kind = Render::CameraKind::Orthographic;
    Eye.XMagM = Eye.YMagM = 2;
    Eye.ZNearM = 0.1;
    Eye.ZFarM = 10;
    Scene->Eye(Eye);
    CHECK(Scene->Watched(), "the old declaration has an explicit camera override");
    return true;
  }

  std::vector<float> Capture() {
    const auto published = Renderer.PublishedWorld();
    CHECK(Scene->Draw(Error), Error.c_str());
    Renderer.WaitForGpu();
    std::vector<float> pixels;
    CHECK(Renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready && !pixels.empty(),
          "the published image is readable");
    return pixels;
  }

  void CheckCandidate(size_t width,
                      bool publish,
                      const std::vector<float> &before,
                      double publishedWidth) {
    {
      Core::WorldCandidate candidate(Renderer);
      const auto prepared = candidate.PrepareDeclaration(Declaration, nullptr, nullptr, {});
      CHECK(prepared,
            prepared ? "the declaration candidate is prepared" : prepared.error().c_str());
      if (!prepared) { return; }
      CHECK(Renderer.PictureW() == publishedWidth,
            "ready declarations leave default renderer queries on the published frame");
      CHECK(candidate.SetGeometry(Replacement.clone(), Replacement.parts(), Error), Error.c_str());
      CHECK(Renderer.PictureW() == publishedWidth,
            "a private edit returns default renderer selection to the published frame");
      CHECK(Capture() == before, "preparation retains the complete published image");
      if (publish) {
        const auto accepted = candidate.Publish(Scene);
        CHECK(accepted, accepted ? "the new declaration is published" : accepted.error().c_str());
        if (!accepted) { return; }
        CHECK(!Scene->Watched(), "a new declaration does not inherit the previous camera override");
        Scene->Eye(Eye);
        const auto after = Capture();
        const size_t channel = width == 32 ? size_t{2} : size_t{0};
        CHECK(Renderer.PictureW() == static_cast<double>(width) && after.size() == width * 32 * 4 &&
                  after[size_t{4} * (size_t{16} * width + width / 2) + channel] > 0.5f &&
                  after != before,
              "publication exposes the requested frame and material");
      }
    }
    CHECK(!Renderer.HasWorldCandidate(), "publication or cancellation releases the private slot");
    if (!publish) { CHECK(Capture() == before, "cancellation preserves the published image"); }
  }

  void CheckWidth(size_t width) {
    const auto uploads = Renderer.OfflineUploads(std::chrono::seconds(5));
    const auto before = Capture();
    const double publishedWidth = Renderer.PictureW();
    Declaration.SurfaceWidthPx = static_cast<int>(width);
    Material material;
    material.Unlit = true;
    if (width == 32) {
      material.BaseColour = {{0, 0, 1, 1}};
    } else {
      material.BaseColour = {{1, 0, 0, 1}};
    }
    CHECK(Replacement.setSurface(Surface, material), "the replacement has a distinct material");
    CheckCandidate(width, false, before, publishedWidth);
    CheckCandidate(width, true, before, publishedWidth);
  }
};
}

int main() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  {
    DeclarationFixture fixture;
    if (fixture.Open()) {
      const auto initial = fixture.Capture();
      CHECK(initial.size() == size_t{32} * 32 * 4 &&
                initial[size_t{4} * (size_t{16} * 32 + 16)] > 0.5f,
            "the published control contains the red triangle");
      fixture.CheckWidth(32);
      fixture.CheckWidth(48);
    }
  }
  SDL_Quit();
  return Report();
}

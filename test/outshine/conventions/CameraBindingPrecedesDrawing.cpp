#include <Outshine.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <unistd.h>
#include "Live.h"
#include "SceneRenderer.h"
#include <cstdint>
#include <vector>
#include "Check.h"

namespace {
outshine::Scenario::Document Declaration(bool withView) {
  outshine::Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {32, 32};
  scene.Render.Outputs = {"surface"};
  if (withView) {
    outshine::Scenario::View view;
    view.Id = "camera-contract";
    view.Person = "first";
    view.Sees.Placed = true;
    view.Sees.Stands.AtM = {{0, 0, 2}};
    view.Sees.setProjection(
        outshine::Scenario::Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
    scene.Views.push_back(view);
  }
  return scene;
}

void ImportedCameraSurvivesBinding() {
  using namespace outshine;
  using namespace outshine::Test;
  std::error_code io;
  const auto directory = std::filesystem::temp_directory_path(io);
  CHECK(!io, "a system temp directory is available for the glTF fixture");
  if (io) { return; }
  const auto path = directory / ("outshine-camera-contract-" + std::to_string(getpid()) + ".gltf");
  std::ofstream file(path);
  file << R"GLTF({
  "asset":{"version":"2.0"},
  "buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AACAvwAAgL8AAAAAAACAPwAAgL8AAAAAAAAAAAAAgD8AAAAA"}],
  "bufferViews":[{"buffer":0,"byteLength":36}],
  "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
  "cameras":[{"type":"perspective","perspective":{"yfov":1,"znear":0.25,"zfar":20}}],
  "nodes":[{"mesh":0},{"camera":0,"translation":[3,4,5]}],
  "scenes":[{"nodes":[0,1]}],"scene":0
})GLTF";
  file.close();
  CHECK(!file.fail(), "the independent glTF camera and triangle fixture is written");
  if (!file.fail()) {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.Stands = path.string();
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    std::unique_ptr<Core::Live> scene;
    std::string error;
    const bool opened = Core::Live::Open(renderer, declaration, nullptr, scene, error);
    CHECK(opened, "the imported camera and mesh bind without an explicit re-framing request");
    if (opened) {
      const Vec3 importedEye = {{3, 4, 5}};
      CHECK(scene->Aimed().EyeM == importedEye && scene->Aimed().YfovRad == 1.0,
            "the initial binding selects the authored glTF camera");
      CHECK(scene->Draw(error), "the initial imported-camera frame draws");
      CHECK(scene->Aimed().EyeM == importedEye && scene->Aimed().ZNearM == 0.25 &&
                scene->Aimed().ZFarM == 20,
            "first drawing preserves the authored camera instead of silently fitting bounds");
      scene->FrameItself();
      const bool rebuilt =
          scene->Restands(path.string(), {}, Scenario::AssetAnimation::Play, 0, error);
      CHECK(rebuilt, "the subject can rebind while an explicit framing request is pending");
      if (rebuilt) {
        CHECK(scene->Draw(error), "the pending framing request draws after rebind");
        CHECK(scene->Aimed().EyeM != importedEye,
              "re-binding did not cancel the explicit request to frame the object bounds");
      }
    }
    if (!error.empty()) { std::printf("imported camera: %s\n", error.c_str()); }
  }
  {
    Geometry native;
    const int part = native.addPart("native default material", MaterialInstance{});
    CHECK(native.setPositions(
              part, std::array<float, 9>{0.8f, -0.5f, 0, 2.2f, -0.5f, 0, 1.5f, 0.5f, 0}) &&
              native.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "native fixture has an intentionally unbound material");
    Core::Declaration declaration;
    declaration.Stands = path.string();
    declaration.InitialGeometry = &native;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Surfacing.front().BaseColour = {{0, 1, 0, 1}};
    declaration.Surfacing.front().Unlit = true;
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error),
          "imported and native geometry share one scene with a default native material");
    if (scene) {
      CHECK(scene->PartsStanding() == 2, "both imported and native parts are retained");
      native.clear();
      Render::Viewpoint eye;
      eye.EyeM = {{0, 0, 5}};
      eye.YfovRad = 1;
      eye.ZNearM = 0.1;
      eye.ZFarM = 100;
      scene->Eye(eye);
      CHECK(scene->Draw(error), "mixed scene survives clearing its native source");
      renderer.WaitForGpu();
      std::vector<float> pixels;
      CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready,
            "mixed material fixture has readable linear pixels");
      const size_t pixel = (16u * 32u + 24u) * 4u;
      CHECK(pixels.size() > pixel + 2u && pixels[pixel] < 0.1f && pixels[pixel + 1u] > 0.9f &&
                pixels[pixel + 2u] < 0.1f,
            "unbound native part uses the declared green material, not an imported slot");
    }
    if (!error.empty()) { std::printf("mixed native material: %s\n", error.c_str()); }
  }
  CHECK(std::filesystem::remove(path, io) && !io, "the temporary camera fixture is removed");
}
} // namespace

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video starts before creating targets");
  if (!initialized) { return Report(); }
  {
    Engine engine;
    const auto target = engine.swapChain();
    CHECK(target.extent().WidthPx == 0 && target.extent().HeightPx == 0,
          "internal default dimensions are not advertised as a configured target");
    CHECK(engine.drawsInto(Extent{32, 32}).has_value(), "configure an offscreen target");
    CHECK(engine.declare(Declaration(false)).has_value(),
          "an empty scene can be declared before a camera is prepared");
    auto renderer = engine.renderer();
    for (int attempt = 0; attempt < 2; ++attempt) {
      const auto result = renderer.render({});
      CHECK(!result && !result.error().empty(),
            "drawing without a camera or derivable bounds returns an error, not an assertion or "
            "false success");
    }
    std::vector<uint8_t> pixels;
    CHECK(!renderer.readPixels(pixels), "readback cannot bypass missing-camera preparation");
    const bool ready = engine.declare(Declaration(true)) && engine.assemble() && engine.advance();
    CHECK(ready, "an explicit view can be prepared after failed draw attempts");
    if (ready) {
      CHECK(renderer.readPixels(pixels).has_value() && pixels.size() == 32u * 32u * 4u,
            "the same renderer recovers and produces a complete RGBA frame");
      constexpr std::array<Extent, 5> invalid = {{{-1, 32}, {0, 32}, {32, 0}, {32, -1}, {48, 32}}};
      for (const Extent frame : invalid) {
        CHECK(!renderer.render(frame),
              "partial, negative and mismatched extents cannot masquerade as the default target");
      }
      CHECK(renderer.render({}).has_value(),
            "invalid extent checks leave the prepared camera usable");
    }
  }
  SDL_Window *window = SDL_CreateWindow("Outshine camera contract", 32, 32, 0);
  CHECK(window != nullptr, "a real presentation target is created");
  if (window != nullptr) {
    {
      Engine engine;
      CHECK(engine.drawsInto(window).has_value(), "the Engine borrows the window");
      CHECK(engine.declare(Declaration(false)).has_value(),
            "a presentation scene starts without a bound camera");
      auto target = engine.swapChain();
      auto renderer = engine.renderer();
      for (int attempt = 0; attempt < 2; ++attempt) {
        const auto begun = renderer.beginFrame(target);
        CHECK(begun.has_value(), "frame setup remains independent of camera selection");
        if (begun) {
          CHECK(!renderer.endFrame(),
                "presentation propagates missing-camera failure before projection math");
        }
      }
      CHECK(!renderer.endFrame(), "the failed presentation did not leave a frame open");
      const bool ready = engine.declare(Declaration(true)) && engine.assemble() && engine.advance();
      CHECK(ready, "camera preparation recovers the existing window target");
      if (ready) {
        const auto begun = renderer.beginFrame(target);
        CHECK(begun.has_value(), "a new frame opens after recovery");
        if (begun) {
          CHECK(renderer.endFrame().has_value(), "the prepared camera presents successfully");
        }
      }
    }
    SDL_DestroyWindow(window);
  }
  ImportedCameraSurvivesBinding();
  SDL_Quit();
  Covers("unconfigured target extent; missing-camera draw/readback/presentation failures and "
         "repeatability; "
         "explicit-view recovery; render extent validation; numerical camera conventions remain in "
         "their own suite");
  return Report();
}

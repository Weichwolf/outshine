#include <Outshine.h>
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <algorithm>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include "Check.h"

namespace {
using namespace outshine;
using namespace outshine::Test;

static_assert(noexcept(std::declval<Camera &>().setProjection(Camera::Perspective{})));
static_assert(noexcept(std::declval<Camera &>().setProjection(Camera::Ortho{})));

Scenario::Document Declaration(bool carried, const std::string &path) {
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {64, 64};
  scene.Render.Outputs = {"sceneLinear", "sceneDepth", "surface"};
  Scenario::Asset asset;
  asset.Kind = "gltf";
  asset.Uri = path;
  scene.Assets.push_back(asset);
  if (carried) {
    Scenario::Body body;
    body.Name = "camera-body";
    body.Asset = path;
    body.Placed = true;
    scene.Bodies.push_back(body);
  }
  const auto add = [&](std::string id, Camera camera) {
    Scenario::View view;
    view.Id = std::move(id);
    view.Sees = camera;
    view.Placement =
        carried ? Scenario::CameraPlacement::FollowEntity : Scenario::CameraPlacement::Local;
    if (carried) {
      view.Follows = "camera-body";
      view.Person = "first";
    }
    scene.Views.push_back(view);
  };
  Camera perspective;
  perspective.setProjection(Camera::Perspective{.FovDeg = 60, .NearM = 0.2, .FarM = 17});
  add("perspective", perspective);
  Camera ortho;
  ortho.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 3, .NearM = 0, .FarM = 17});
  add("ortho", ortho);
  add("defaults", {});
  const auto bad = [&](Camera camera) { add("bad-" + std::to_string(scene.Views.size()), camera); };
  for (const auto field : {&Camera::FovDeg, &Camera::NearM, &Camera::FarM}) {
    for (double value : {-1.0, std::numeric_limits<double>::quiet_NaN()}) {
      auto camera = perspective;
      camera.*field = value;
      bad(camera);
    }
  }
  for (const auto field : {&Camera::XMagM, &Camera::YMagM}) {
    for (double value : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
      auto camera = ortho;
      camera.*field = value;
      bad(camera);
    }
  }
  auto camera = ortho;
  camera.NearM = -1;
  bad(camera);
  camera = ortho;
  camera.FarM = std::numeric_limits<double>::infinity();
  bad(camera);
  camera = perspective;
  camera.NearM = 1;
  camera.FarM = std::nextafter(1.0, 2.0);
  bad(camera);
  return scene;
}

struct Frame {
  std::vector<float> Colour;
  std::vector<float> Depth;
};

Frame Capture(Engine &engine) {
  Frame frame;
  CHECK(engine.renderer().readPixels(Buffer::Linear, frame.Colour).has_value(),
        "read the actual linear frame through the public renderer");
  CHECK(engine.renderer().readPixels(Buffer::Depth, frame.Depth).has_value(),
        "read the actual reverse-Z depth through the public renderer");
  return frame;
}

Frame CheckProjection(Engine &engine, const Camera &camera) {
  auto frame = Capture(engine);
  CHECK(frame.Colour.size() == 64 * 64 * 4 && frame.Depth.size() == 64 * 64,
        "both attachments cover the declared viewport");
  if (frame.Colour.size() != 64 * 64 * 4 || frame.Depth.size() != 64 * 64) { return frame; }
  const double perspectiveScale = 2 * std::tan(camera.FovDeg * std::numbers::pi / 360);
  const double scaleX = camera.Orthographic ? camera.XMagM : perspectiveScale;
  const double scaleY = camera.Orthographic ? camera.YMagM : perspectiveScale;
  const double guard = 2 * std::max(scaleX, scaleY) / 64;
  const double depth = camera.Orthographic ? (camera.FarM - 2) / (camera.FarM - camera.NearM)
                       : camera.FarM == 0
                           ? camera.NearM / 2
                           : camera.NearM * (camera.FarM - 2) / (2 * (camera.FarM - camera.NearM));
  size_t inside = 0;
  size_t outside = 0;
  bool coverage = true;
  double worst = 0;
  for (size_t row = 0; row < 64; ++row) {
    for (size_t column = 0; column < 64; ++column) {
      const double x = (2 * (static_cast<double>(column) + 0.5) / 64 - 1) * scaleX;
      const double y = (1 - 2 * (static_cast<double>(row) + 0.5) / 64) * scaleY;
      const double halfWidth = (1 - y) / 2;
      if (std::abs(y + 1) <= guard || std::abs(y - 1) <= guard ||
          std::abs(std::abs(x) - halfWidth) <= guard) {
        continue;
      }
      const bool hit = y > -1 && y < 1 && std::abs(x) < halfWidth;
      const size_t at = row * 64 + column;
      coverage = coverage && (frame.Colour[at * 4] > 0) == hit;
      if (hit) {
        ++inside;
        worst = std::max(worst, std::abs(frame.Depth[at] - depth));
      } else {
        ++outside;
      }
    }
  }
  CHECK(inside > 0 && outside > 0 && coverage,
        "pixel centres away from raster edges match the analytic projected triangle");
  CHECK_NEAR(worst,
             0.0,
             1e-5,
             "reverse-Z",
             "rendered depth matches the declared near/far planes at two metres");
  return frame;
}

std::vector<float> Exercise(bool carried, const std::string &path) {
  Engine engine;
  const auto declared = Declaration(carried, path);
  const bool ready = engine.drawsInto(Extent{64, 64}) && engine.declare(declared) &&
                     engine.assemble() && engine.advance();
  CHECK(ready, "a fully declared camera binds through the public Engine API");
  if (!ready) {
    std::printf("camera setup: %s\n", engine.error().c_str());
    return {};
  }
  CheckProjection(engine, declared.Views[0].Sees);
  CHECK(engine.setView("ortho") && engine.advance(), "select and prepare orthographic projection");
  const auto ortho = CheckProjection(engine, declared.Views[1].Sees);
  std::error_code io;
  const auto directory = std::filesystem::temp_directory_path(io);
  CHECK(!io, "a system temp directory is available for the visual evidence");
  if (!io) {
    const auto png =
        directory / (carried ? "outshine-camera-carried.png" : "outshine-camera-standing.png");
    CHECK(engine.renderer().saveScreenshot(png.string()).has_value(),
          "save the actual orthographic image for visual inspection");
  }
  CHECK(engine.setView("defaults") && engine.advance(),
        "prepare the documented perspective defaults");
  Camera defaults;
  defaults.setProjection(Camera::Perspective{
      .FovDeg = Scenario::kFovUnsaidDeg, .NearM = Camera::kNearestM, .FarM = 0});
  const auto previous = CheckProjection(engine, defaults);
  for (const auto &view : declared.Views) {
    if (!view.Id.starts_with("bad-")) { continue; }
    CHECK(engine.setView(view.Id).has_value(), "select a view before its projection is prepared");
    for (int attempt = 0; attempt < 2; ++attempt) {
      const auto advanced = engine.advance();
      CHECK(!advanced && !advanced.error().empty(),
            "invalid projection repeatedly returns an error");
      const auto retained = Capture(engine);
      CHECK(retained.Colour == previous.Colour && retained.Depth == previous.Depth,
            "failed projection preparation preserves the last rendered projection");
    }
    CHECK(engine.setView("defaults") && engine.advance(),
          "recover by selecting a valid declared view");
  }
  return ortho.Colour;
}
} // namespace

int main() {
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video starts before camera rendering");
  if (!initialized) { return Report(); }
  std::error_code io;
  const auto directory = std::filesystem::temp_directory_path(io);
  CHECK(!io, "a system temp directory holds the independent glTF fixture");
  if (io) {
    SDL_Quit();
    return Report();
  }
  const auto path = directory / "outshine-scenario-camera.gltf";
  std::ofstream file(path);
  file << R"GLTF({
    "asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_unlit"],
    "buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AACAvwAAgL8AAADAAACAPwAAgL8AAADAAAAAAAAAgD8AAADA"}],
    "bufferViews":[{"buffer":0,"byteLength":36}],
    "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,-2],"max":[1,1,-2]}],
    "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0.25,0.05,1]},"extensions":{"KHR_materials_unlit":{}}}],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],
    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0
  })GLTF";
  file.close();
  CHECK(!file.fail(), "write the independently specified triangle at two metres");
  const auto standing = Exercise(false, path.string());
  const auto carried = Exercise(true, path.string());
  CHECK(!standing.empty() && standing == carried,
        "standing and carried orthographic cameras produce identical linear images");
  bool lit = false;
  for (size_t at = 0; at + 3 < standing.size(); at += 4) { lit = lit || standing[at] > 0; }
  CHECK(lit, "the comparison contains rendered geometry, not two empty images");
  CHECK(std::filesystem::remove(path, io) && !io, "remove the temporary glTF fixture");
  SDL_Quit();
  return Report();
}

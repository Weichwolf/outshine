#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Viewing.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace outshine;
namespace Native = Render::Native;
constexpr const char *kWaitFailure = "injected synchronous geometry completion failure";
constexpr double kNearM = 0.1;
constexpr double kFarM = 100;

struct QueueState {
  Native::Device *Backing = nullptr;
  std::optional<Native::Completion> Last;
  size_t BlockedBegins = 0;
  bool Pressure = false;
  bool NeedsWait = false;
  bool FailWait = false;
};

class Pressured final : public Test::Gpu::NativeDeviceProxy {
public:
  Pressured(std::unique_ptr<Native::Device> device, QueueState &state)
      : NativeDeviceProxy(*device), Owned_(std::move(device)), State_(state) {
    State_.Backing = Owned_.get();
  }

  static Native::Result<std::unique_ptr<Native::Device>> Create(void *context,
                                                                Native::DeviceOptions options) {
    options.Validation = true;
    auto device = Native::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<Pressured>(std::move(*device), *static_cast<QueueState *>(context));
  }

  Native::Result<std::unique_ptr<Native::Commands>> Begin() override {
    if (State_.Pressure && State_.NeedsWait) {
      ++State_.BlockedBegins;
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                           .Message = "accepted upload completion is deferred"});
    }
    return Backing().Begin();
  }

  Native::Result<Native::Completion> Submit(std::unique_ptr<Native::Commands> commands) override {
    auto submitted = Backing().Submit(std::move(commands));
    if (submitted) {
      State_.Last = *submitted;
      State_.NeedsWait = true;
    }
    return submitted;
  }

  Native::Result<Native::CompletionStatus> Wait(Native::Completion completion,
                                                std::chrono::nanoseconds budget) override {
    if (State_.FailWait) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Driver, .Message = kWaitFailure});
    }
    auto status = Backing().Wait(completion, budget);
    if (status && *status == Native::CompletionStatus::Ready) { State_.NeedsWait = false; }
    return status;
  }

private:
  std::unique_ptr<Native::Device> Owned_;
  QueueState &State_;
};

Geometry Triangle() {
  Geometry geometry;
  const auto surface = geometry.addSurface("surface", Material{});
  CHECK(surface, "the synchronous product has a material");
  if (!surface) { return geometry; }
  const auto part = geometry.addPart("triangle", *surface);
  CHECK(part && geometry.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the synchronous product contains complete native triangles");
  return geometry;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    QueueState state;
    Render::SceneRenderer renderer({.Context = &state, .Create = Pressured::Create});
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
    if (scene) {
      state.Pressure = true;
      CHECK(scene->SetGeometry(Triangle(), 0, error), error.c_str());
      CHECK(state.BlockedBegins > 0 && !state.NeedsWait && state.Last,
            "synchronous construction drains accepted uploads despite queue backpressure");
      if (state.Last) {
        const auto ready = state.Backing->Poll(*state.Last);
        CHECK(ready && *ready == Native::CompletionStatus::Ready,
              "the actual final GPU completion is ready before the synchronous call returns");
      }
      Render::Viewpoint eye;
      eye.EyeM = {{0, 0, 4}};
      eye.YfovRad = 1;
      eye.ZNearM = kNearM;
      eye.ZFarM = kFarM;
      scene->Eye(eye);
      CHECK(scene->Advance(error) && scene->Draw(error),
            "the first frame can acquire the device after synchronous construction");
      CHECK(renderer.Settle(error), "the accepted frame completes before the failure case");
      state.FailWait = true;
      CHECK(!scene->SetGeometry(Triangle(), 0, error) && error == kWaitFailure,
            "native completion failure reaches the synchronous geometry caller unchanged");
      state.FailWait = false;
      CHECK(renderer.Settle(error), "accepted resources remain drainable after the reported error");
    }
  }
  SDL_Quit();
  return Report();
}

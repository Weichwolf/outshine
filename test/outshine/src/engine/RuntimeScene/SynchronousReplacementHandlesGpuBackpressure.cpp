#include "RuntimeScene.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
namespace Gpu = Render::Native;

struct Progress {
  unsigned Busy = 0, Waits = 0;
};

class DelayedDevice final : public Test::Gpu::NativeDeviceProxy {
public:
  DelayedDevice(std::unique_ptr<Gpu::Device> device, Progress &progress)
      : NativeDeviceProxy(*device), Device_(std::move(device)), Progress_(progress) {}

  static Gpu::Result<std::unique_ptr<Gpu::Device>> Create(void *context,
                                                          Gpu::DeviceOptions options) {
    auto device = Gpu::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<DelayedDevice>(std::move(*device), *static_cast<Progress *>(context));
  }

  Gpu::Result<std::unique_ptr<Gpu::Commands>> Begin() override {
    if (Pending_) {
      ++Progress_.Busy;
      return std::unexpected(
          Gpu::Error{.Kind = Gpu::ErrorKind::Busy, .Message = "publication needs GPU progress"});
    }
    return Backing().Begin();
  }

  Gpu::Result<Gpu::Completion> Submit(std::unique_ptr<Gpu::Commands> commands) override {
    auto result = Backing().Submit(std::move(commands));
    if (result) { Pending_ = *result; }
    return result;
  }

  Gpu::Result<Gpu::CompletionStatus> Poll(Gpu::Completion completion) override {
    if (Pending_ && completion.Serial >= Pending_->Serial) {
      return Gpu::CompletionStatus::Pending;
    }
    return Backing().Poll(completion);
  }

  Gpu::Result<Gpu::CompletionStatus> Wait(Gpu::Completion completion,
                                          std::chrono::nanoseconds budget) override {
    if (budget <= std::chrono::nanoseconds::zero()) { return Poll(completion); }
    ++Progress_.Waits;
    auto result = Backing().Wait(completion, budget);
    if (result && *result == Gpu::CompletionStatus::Ready && Pending_ &&
        completion.Serial >= Pending_->Serial) {
      Pending_.reset();
    }
    return result;
  }

private:
  std::unique_ptr<Gpu::Device> Device_;
  Progress &Progress_;
  std::optional<Gpu::Completion> Pending_;
};

Geometry Triangle() {
  Geometry geometry;
  Material material;
  material.Unlit = true;
  material.DoubleSided = true;
  material.BaseColour = {{.25f, .5f, .75f, 1}};
  const auto surface = geometry.addSurface("replacement", material);
  if (!surface) { return {}; }
  const auto part = geometry.addPart("triangle", *surface);
  if (!part || !geometry.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) ||
      !geometry.setNormals(*part, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}) ||
      !geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2})) {
    return {};
  }
  return geometry;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for synchronous GPU publication");
  {
    Progress progress;
    Render::SceneRenderer renderer({.Context = &progress, .Create = DelayedDevice::Create});
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    std::unique_ptr<Core::RuntimeScene> live;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, live, error),
          "initial synchronous publication handles finite GPU pressure");
    if (!live) { return Report(); }
    auto camera = Render::Viewpoint::LookAt({.EyeM = {{0, 0, 4}}, .AimM = {}}, 0.0);
    CHECK(camera.has_value(), "replacement has an explicit camera");
    if (!camera) { return Report(); }
    camera->Kind = Render::CameraKind::Orthographic;
    camera->XMagM = camera->YMagM = 2;
    camera->ZNearM = .1;
    camera->ZFarM = 10;
    live->Eye(*camera);
    const auto waits = progress.Waits;
    const auto busy = progress.Busy;
    const bool replaced =
        Core::RuntimeScene::ReplacesGeometry(renderer, *live, Triangle(), nullptr, live, error);
    CHECK(replaced, "synchronous replacement progresses through upload backpressure");
    if (!replaced) {
      std::println(stderr, "replacement error: {}", error);
      return Report();
    }
    CHECK(progress.Busy > busy && progress.Waits > waits,
          "replacement exercises actual Busy acquisition and bounded progress");
    CHECK(live->Draw(error), "published replacement draws immediately without an extra warmup");
    renderer.WaitForGpu();
    std::vector<float> pixels;
    CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready,
          "published GPU radiance can be read");
    constexpr size_t centre = (size_t{16} * 32 + 16) * 4;
    CHECK(pixels.size() == size_t{32} * 32 * 4 && pixels[centre] > .1f &&
              pixels[centre + 1] > .1f && pixels[centre + 2] > .1f,
          "the replacement contains visible radiance");
  }
  SDL_Quit();
  return Report();
}

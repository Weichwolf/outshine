#include "MaterialFieldPass.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include "test/harness/render/NativeMaterialField.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct Tracking final : NativeDeviceProxy {
  explicit Tracking(Device &device) : NativeDeviceProxy(device) {}

  bool RefuseDepth = false, CancelAtDepth = false;
  std::atomic_bool Stop{false};
  size_t Depths = 0, Submissions = 0;

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    if (description.Texels == Format::Depth32Float) {
      if (RefuseDepth) {
        return std::unexpected(Error{.Kind = ErrorKind::OutOfMemory, .Message = "depth rejected"});
      }
      ++Depths;
      if (CancelAtDepth) { Stop.store(true, std::memory_order_relaxed); }
    }
    return Backing().CreateTexture(description, label);
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submissions;
    return Backing().Submit(std::move(commands));
  }
};

std::vector<float> DepthPixels(Device &device, const MaterialFieldImages &images) {
  const auto &shape = images.Depth->Description();
  CHECK(shape.Texels == Format::Depth32Float &&
            shape.Width == images.Unfiltered->Texture->Description().Width &&
            shape.Height == images.Unfiltered->Texture->Description().Height,
        "raw colour and depth share one full-resolution raster");
  const auto bytes =
      ReadImage(device, *images.Depth, {.Width = shape.Width, .Height = shape.Height});
  std::vector<float> result(size_t{shape.Width} * shape.Height);
  for (size_t row = 0; row < shape.Height; ++row) {
    std::memcpy(result.data() + row * shape.Width,
                bytes.Rows.data() + row * bytes.RowBytes,
                size_t{shape.Width} * sizeof(float));
  }
  return result;
}

void Exercise() {
  auto native = Require(CreateDevice({.Validation = true}));
  Tracking device(*native);
  MaterialFieldImages first;
  MaterialFieldImages second;
  std::vector<float> held;
  {
    MaterialFieldPass pass(device);
    Require(pass.Configure());
    const auto input = PreparedMaterialField({.4f, .5f, .6f, 1}, true);
    const auto source =
        Require(RetryOffline([&] { return pass.Prepare(input); }, std::chrono::seconds(5)));
    CHECK(device.Depths == 0, "resident material geometry allocates no unused depth target");
    MaterialFieldView view;
    view.Camera.Kind = CameraKind::Orthographic;
    view.Camera.EyeM = {{0, 0, 2}};
    view.Camera.XMagM = view.Camera.YMagM = 1;
    view.Camera.ZNearM = .1;
    view.Camera.ZFarM = 10;
    view.Target = {.WidthPx = 16, .HeightPx = 16};
    first =
        Require(RetryOffline([&] { return pass.Relight(*source, view); }, std::chrono::seconds(5)));
    held = DepthPixels(device, first);
    CHECK(first.Filtered->Texture->Description().Width == 16 && held.size() == size_t{32} * 32,
          "filtered colour does not mislabel raw depth as reduced depth");
    CHECK(first.Camera.EyeM == view.Camera.EyeM && first.Camera.Kind == view.Camera.Kind &&
              first.Camera.Right == view.Camera.Right && first.Camera.Up == view.Camera.Up &&
              first.Camera.Forward == view.Camera.Forward && first.Camera.XMagM == 1 &&
              first.Camera.YMagM == 1 && first.Camera.ZNearM == .1 && first.Camera.ZFarM == 10,
          "the retained depth carries its actual raster projection");
    const size_t centre = 16 * 32 + 16;
    const auto tolerance = 8 * std::numeric_limits<float>::epsilon();
    CHECK(std::abs(held[centre] - (10.0 - 2.0) / (10.0 - .1)) < tolerance && held[0] == 0,
          "orthographic reverse-Z represents the original plane and leaves background clear");
    view.Camera.EyeM[2] = 3;
    second =
        Require(RetryOffline([&] { return pass.Relight(*source, view); }, std::chrono::seconds(5)));
    const auto moved = DepthPixels(device, second);
    CHECK(second.Depth != first.Depth && device.Depths == 2,
          "two held field views own separate depth backing");
    CHECK(std::abs(moved[centre] - (10.0 - 3.0) / (10.0 - .1)) < tolerance &&
              second.Camera.EyeM == view.Camera.EyeM && first.Camera.EyeM[2] == 2,
          "camera movement changes depth without changing the older projection");
    CHECK(DepthPixels(device, first) == held, "a later relight cannot overwrite held depth");
    CHECK(first.Unfiltered->Produced.Serial == first.Filtered->Produced.Serial &&
              first.Unfiltered->Produced.Device == first.Filtered->Produced.Device,
          "raw colour, depth and filtering complete in their one producing submission");
    const auto submitted = device.Submissions;
    const auto last = second.Unfiltered->Produced;
    const auto unchanged = [&] {
      const auto current = pass.LastSubmission();
      return current && current->Device == last.Device && current->Serial == last.Serial;
    };
    device.RefuseDepth = true;
    const auto rejected = pass.Relight(*source, view);
    CHECK(!rejected && rejected.error().Kind == ErrorKind::OutOfMemory &&
              device.Submissions == submitted && unchanged(),
          "failed depth allocation publishes no partial field and retains its native error");
    device.RefuseDepth = false;
    device.CancelAtDepth = true;
    const auto cancelled = pass.Relight(*source, view, &device.Stop);
    CHECK(!cancelled && cancelled.error().Kind == ErrorKind::Cancelled &&
              device.Submissions == submitted && unchanged(),
          "cancellation after depth allocation discards the entire unsubmitted view");
    device.CancelAtDepth = false;
    device.Stop.store(false, std::memory_order_relaxed);
    const auto retried =
        Require(RetryOffline([&] { return pass.Relight(*source, view); }, std::chrono::seconds(5)));
    CHECK(DepthPixels(device, retried) == moved && DepthPixels(device, first) == held,
          "a cancelled recording can retry without damaging an older image or camera");
    Require(pass.Collect());
  }
  CHECK(DepthPixels(device, first) == held,
        "held image depth survives source, pass and producer retirement");
  const std::weak_ptr<Texture> depth = first.Depth;
  first = {};
  CHECK(depth.expired(), "the last image lease releases its retired depth wrapper");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native depth resources");
  Exercise();
  SDL_Quit();
  return Report();
}

#include "MaterialFieldPass.h"
#include "Check.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include "test/harness/render/NativeMaterialField.h"
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
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

  bool RefuseIntegral = false;
  size_t Submissions = 0;

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    if (RefuseIntegral && description.Texels == Format::Rgba32Float) {
      return std::unexpected(Error{.Kind = ErrorKind::OutOfMemory, .Message = "integral rejected"});
    }
    return Backing().CreateTexture(description, label);
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submissions;
    return Backing().Submit(std::move(commands));
  }
};

std::array<double, 4> SourceSum(Device &device, const Texture &source) {
  const auto &shape = source.Description();
  auto rows = ReadImage(device, source, {.Width = shape.Width, .Height = shape.Height});
  std::vector<float> pixels(size_t{shape.Width} * shape.Height * 4);
  CHECK(SDL_ConvertPixels(shape.Width,
                          shape.Height,
                          SDL_PIXELFORMAT_RGBA64_FLOAT,
                          rows.Rows.data(),
                          static_cast<int>(rows.RowBytes),
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          pixels.data(),
                          shape.Width * 16),
        "the independent reference decodes only the unfiltered source");
  std::array<double, 4> sum{};
  for (size_t at = 0; at < pixels.size(); ++at) { sum[at % 4] += pixels[at]; }
  return sum;
}

void Exercise() {
  auto native = Require(CreateDevice({.Validation = true}));
  Tracking device(*native);
  MaterialFieldImages held;
  {
    MaterialFieldPass pass(device);
    Require(pass.Configure());
    auto input = PreparedMaterialField({.5f, .25f, .75f, 1}, true);
    auto source = Require(pass.Prepare(input));
    MaterialFieldView view;
    view.Camera.Kind = CameraKind::Orthographic;
    view.Camera.EyeM = {{0, 0, 2}};
    view.Camera.XMagM = view.Camera.YMagM = 1;
    view.Camera.ZNearM = .1;
    view.Camera.ZFarM = 10;
    view.Target = {.WidthPx = 32, .HeightPx = 32};
    view.Encoding = RadianceEncoding::IntegralPairs;
    held = Require(pass.Relight(*source, view));
    CHECK(held.Filtered->Encoding == RadianceEncoding::IntegralPairs &&
              held.Unfiltered->Encoding == RadianceEncoding::Texels &&
              held.Filtered->Texture->Description().Width == 66 &&
              held.Filtered->Texture->Description().Texels == Format::Rgba32Float &&
              held.Depth->Description().Width == 32,
          "integral output keeps its format, zero border and separate raw depth raster");
    CHECK(held.Unfiltered->Produced.Serial == held.Filtered->Produced.Serial,
          "source shading and prefix generation share one producer completion");
    device.RefuseIntegral = true;
    const auto submissions = device.Submissions;
    const auto refused = pass.Relight(*source, view);
    CHECK(!refused && refused.error().Kind == ErrorKind::OutOfMemory &&
              device.Submissions == submissions,
          "integral allocation failure publishes no provisional image or submission");
    device.RefuseIntegral = false;
    const std::atomic_bool stopped{true};
    CHECK(!pass.Relight(*source, view, &stopped) && device.Submissions == submissions,
          "cancelled integration keeps the last accepted result");
    view.Target.WidthPx = 16;
    CHECK(!pass.Relight(*source, view) && device.Submissions == submissions,
          "a prefix cannot silently change its source raster");
    source.reset();
    input.reset();
    CHECK(Require(device.Wait(held.Filtered->Produced, std::chrono::seconds(5))) ==
              CompletionStatus::Ready,
          "the offline consumer drains the shared completion");
    Require(pass.Collect());
  }
  const auto expected = SourceSum(device, *held.Unfiltered->Texture);
  const auto rows = ReadImage(device, *held.Filtered->Texture, {.Width = 66, .Height = 33});
  std::array<float, 8> actual{};
  std::memcpy(
      actual.data(), rows.Rows.data() + 32 * rows.RowBytes + 32 * sizeof(actual), sizeof(actual));
  for (size_t channel = 0; channel < 4; ++channel) {
    CHECK(
        double(actual[channel]) + actual[channel + 4] == expected[channel],
        "held integral survives source/pass retirement and matches independent HDR/coverage sums");
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes native material pipelines");
  Exercise();
  SDL_Quit();
  return Report();
}

#include <expected>
#include "MaterialFieldPass.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include "test/harness/render/NativeMaterialField.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <optional>
#include <thread>
#include <span>
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

  bool Busy = false;
  size_t Buffers = 0, Submits = 0, Waits = 0;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    if (description.Placement != Memory::Readback) { ++Buffers; }
    return Backing().CreateBuffer(description, label);
  }

  Result<std::unique_ptr<Commands>> Begin() override {
    if (Busy) { return std::unexpected(Error{.Kind = ErrorKind::Busy, .Message = "queue full"}); }
    return Backing().Begin();
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submits;
    return Backing().Submit(std::move(commands));
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    ++Waits;
    return Backing().Wait(completion, budget);
  }
};

MaterialFieldView View(std::span<const SceneLight> lights) {
  MaterialFieldView view;
  view.Camera.Kind = CameraKind::Orthographic;
  view.Camera.EyeM = {{0, 0, 2}};
  view.Camera.XMagM = view.Camera.YMagM = 1;
  view.Camera.ZNearM = .1;
  view.Camera.ZFarM = 10;
  view.Lights = lights;
  view.Target = {.WidthPx = 16, .HeightPx = 16};
  return view;
}

std::vector<float> Pixels(Device &device, const RadianceImage &image) {
  const auto &shape = image.Texture->Description();
  const auto rows =
      ReadImage(device, *image.Texture, {.Width = shape.Width, .Height = shape.Height});
  std::vector<float> result(size_t{shape.Width} * shape.Height * 4);
  CHECK(SDL_ConvertPixels(static_cast<int>(shape.Width),
                          static_cast<int>(shape.Height),
                          SDL_PIXELFORMAT_RGBA64_FLOAT,
                          rows.Rows.data(),
                          static_cast<int>(rows.RowBytes),
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          result.data(),
                          static_cast<int>(shape.Width * 16)),
        "GPU radiance converts without display transfer");
  return result;
}

MaterialFieldImages Relight(Tracking &device,
                            MaterialFieldPass &pass,
                            const ResidentMaterialField &source,
                            const MaterialFieldView &view) {
  const auto waits = device.Waits;
  auto result =
      Require(RetryOffline([&] { return pass.Relight(source, view); }, std::chrono::seconds(5)));
  CHECK(device.Waits == waits, "admitted relighting never waits on the GPU");
  CHECK(result.Unfiltered->Produced.Device == result.Filtered->Produced.Device &&
            result.Unfiltered->Produced.Serial == result.Filtered->Produced.Serial,
        "geometry and filtering share one accepted producer completion");
  return result;
}

void Exercise() {
  auto native = Require(CreateDevice({.Validation = true}));
  Tracking device(*native);
  MaterialFieldPass pass(device);
  Require(pass.Configure());
  auto a = PreparedMaterialField({1, .125f, .125f, 1});
  auto b = PreparedMaterialField({.125f, 1, .125f, 1});
  const auto sourceA =
      Require(RetryOffline([&] { return pass.Prepare(a); }, std::chrono::seconds(5)));
  const auto sourceB =
      Require(RetryOffline([&] { return pass.Prepare(b); }, std::chrono::seconds(5)));
  const std::weak_ptr<const MaterialFieldInput> cpu = a;
  a.reset();
  CHECK(cpu.expired(), "GPU source ownership does not retain decoded CPU field payloads");
  std::array<SceneLight, 1> lights{};
  lights[0].Light.Intensity = 100;
  auto view = View(lights);
  const auto buffers = device.Buffers;
  const auto first = Relight(device, pass, *sourceA, view);
  const auto other = Relight(device, pass, *sourceB, view);
  const auto red = Pixels(device, *first.Filtered);
  const auto green = Pixels(device, *other.Filtered);
  const size_t centre = (size_t{8} * 16 + 8) * 4;
  CHECK(red[centre] > red[centre + 1] && green[centre + 1] > green[centre],
        "two resident fields retain independent physical material colour");
  CHECK(red[centre + 3] == 1 && red[3] == 0 && green[centre + 3] == 1,
        "covered and uncovered pixels keep exact coupled coverage");
  lights[0].Light.Intensity = 200;
  const auto brighter = Relight(device, pass, *sourceA, view);
  const auto bright = Pixels(device, *brighter.Filtered);
  CHECK(std::abs(bright[centre] - 2 * red[centre]) < bright[centre] * .002f,
        "doubling incident illumination doubles linear PBR radiance");
  view.Camera.EyeM[0] = .75;
  const auto moved = Relight(device, pass, *sourceA, view);
  const auto shifted = Pixels(device, *moved.Filtered);
  CHECK(shifted != bright, "a field-local view update changes its rasterized support");
  CHECK(device.Buffers == buffers, "light and camera changes do not reupload source buffers");
  const std::atomic_bool stopped{true};
  const auto submits = device.Submits;
  auto cancelled = pass.Relight(*sourceA, view, &stopped);
  CHECK(!cancelled && cancelled.error().Kind == ErrorKind::Cancelled && device.Submits == submits,
        "a cancelled relight leaves the previous valid field output untouched");
  lights[0].Light.Intensity = std::numeric_limits<float>::quiet_NaN();
  auto invalid = pass.Relight(*sourceA, view);
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid && device.Submits == submits,
        "nonfinite illumination cannot publish an incomplete or nonfinite field");
  lights[0].Light.Intensity = 200;
  std::optional<ErrorKind> foreign;
  {
    const std::jthread worker([&] {
      const auto result = pass.Relight(*sourceA, view);
      if (!result) { foreign = result.error().Kind; }
    });
  }
  CHECK(foreign == ErrorKind::Invalid && device.Submits == submits,
        "field GPU work refuses a non-owner worker before touching the device");
  device.Busy = true;
  const auto refused = pass.Relight(*sourceA, view);
  CHECK(!refused && refused.error().Kind == ErrorKind::Busy,
        "backend admission remains a typed nonblocking Busy result");
  device.Busy = false;
  view.Target = {.WidthPx = 7, .HeightPx = 7};
  CHECK(!pass.Relight(*sourceA, view), "partial integer footprints publish no output");
  auto empty = MaterialFieldInput::Prepare(Geometry{}, {.WidthPx = 32, .HeightPx = 32});
  CHECK(!empty, "empty source fields are refused rather than published as complete");
  if (const auto last = pass.LastSubmission()) {
    CHECK(Require(device.Wait(*last, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "the offline driver explicitly drains its final source submission");
  }
  Require(pass.Collect());
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native shader setup");
  Exercise();
  SDL_Quit();
  return Report();
}

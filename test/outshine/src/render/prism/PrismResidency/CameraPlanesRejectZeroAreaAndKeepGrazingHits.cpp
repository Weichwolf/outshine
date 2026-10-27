#include "Check.h"
#include "PrismDrawing.h"
#include "PrismPipelines.h"
#include "PrismResidency.h"
#include "UploadQueue.h"
#include "math/Mat4.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 96, kHeight = 80;

PrismDepthView View(float focal, float cameraX, float cameraY) {
  PrismDepthView result;
  result.ClipFromWorld.Column.fill(0);
  result.ClipFromWorld.At(0, 0) = focal;
  result.ClipFromWorld.At(1, 1) = focal;
  result.ClipFromWorld.At(0, 3) = -focal * cameraX;
  result.ClipFromWorld.At(1, 3) = -focal * cameraY;
  result.ClipFromWorld.At(2, 3) = .25f;
  result.ClipFromWorld.At(3, 2) = -1;
  return result;
}

ImagePixels Draw(Device &device,
                 const GraphicsPipeline &pipeline,
                 Texture &target,
                 const PrismDepthView &view,
                 float translationX) {
  std::chrono::nanoseconds budget = std::chrono::seconds(5);
  UploadQueue uploads(device, budget);
  PrismResidency residency;
  Require(residency.Uses(device, &uploads));
  const auto source = std::make_shared<const PrismGrid>(PrismGrid{
      .Samples =
          std::make_shared<const std::vector<PrismSpan>>(1, PrismSpan{.Lower = -1, .Upper = 1}),
      .Patches = {{.Columns = 1, .Rows = 1, .PitchX = 2, .PitchZ = 2, .FillFraction = 1}}});
  PrismInstance instance;
  instance.Current.At(0, 3) = translationX;
  instance.Current.At(2, 3) = -1;
  instance.Previous = instance.Current;
  Require(residency.Place(source, std::span(&instance, 1), 0));
  auto commands = Require(uploads.Begin());
  Require(commands->BeginRenderPass(
      {.Colours = {},
       .Depth = DepthAttachment{
           .Image = {.Image = &target}, .Begin = Load::Clear, .End = Store::Store, .Clear = 0}}));
  Require(EncodePrismDepth(residency,
                           *commands,
                           pipeline,
                           view,
                           {.Width = kWidth, .Height = kHeight},
                           {.Width = kWidth, .Height = kHeight}));
  Require(commands->EndRenderPass());
  const auto complete = Require(SubmitUpload(device, &uploads, std::move(commands)));
  CHECK(Require(device.Wait(complete, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "actual product depth shader completes grazing and camera-plane queries");
  return ReadImage(device, target, {.Width = kWidth, .Height = kHeight});
}

void Compare(const ImagePixels &image, double distance, double focal) {
  bool finite = true;
  size_t hits = 0;
  size_t mismatches = 0;
  double largestError = 0;
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      float actual = 0;
      std::memcpy(
          &actual, image.Rows.data() + y * image.RowBytes + x * sizeof(float), sizeof(float));
      finite = finite && std::isfinite(actual);
      double expected = 0;
      if (distance > 0) {
        const double qx = 2 * (static_cast<double>(x) + .5) / kWidth - 1;
        const double t = distance * focal / qx;
        if (t > .25 && t < 1) { expected = .25 / t; }
      }
      hits += actual > 0 ? 1u : 0u;
      mismatches += ((actual > 0) != (expected > 0)) ? 1u : 0u;
      largestError = std::max(largestError, std::abs(static_cast<double>(actual) - expected));
    }
  }
  CHECK(finite, "all actual depth pixels remain finite");
  CHECK(mismatches == 0, "only independently projected positive-area face pixels reach depth");
  CHECK(largestError < 2e-6, "original near plane and side-face intersection preserve depth");
  CHECK(distance > 0 ? hits > 2000 : hits == 0,
        "exact camera planes are empty while arbitrarily close nonzero faces remain visible");
}

void Probe(Device &device) {
  PrismPipelines pipelines;
  Require(pipelines.ConfigureDepth(device));
  auto target =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  for (const int axis : {0, 1}) {
    const auto view = View(0x1p24f, axis == 0 ? 1.f : 0.f, axis == 1 ? 1.f : 0.f);
    Compare(Draw(device, *pipelines.Depth(), *target, view, -1), 0, 0);
  }
  for (const int exponent : {8, 16, 24, 32, 68, 100}) {
    const float distance = std::ldexp(1.f, -exponent);
    const float focal = .25f / distance;
    const auto view = View(focal, -distance, 0);
    Compare(Draw(device, *pipelines.Depth(), *target, view, 0), distance, focal);
  }
  for (const int exponent : {-64, -32, 32, 64}) {
    auto view = View(64, -0x1p-8f, 0);
    for (float &value : view.ClipFromWorld.Column) { value = std::ldexp(value, exponent); }
    Compare(Draw(device, *pipelines.Depth(), *target, view, 0), 0x1p-8, 64);
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  Probe(*device);
  return Report();
}

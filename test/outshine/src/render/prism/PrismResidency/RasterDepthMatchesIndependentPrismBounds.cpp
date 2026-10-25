#include "PrismDrawing.h"
#include "PrismResidency.h"
#include "PrismPipelines.h"
#include "UploadQueue.h"
#include "Mat4.h"
#include "Vec3.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr uint32_t kWidth = 144;
constexpr uint32_t kHeight = 96;
constexpr float kWorldWidth = 18;
constexpr float kWorldDepth = 12;
constexpr float kVerticalHalf = 20;

struct WorldBox {
  std::array<double, 3> Low;
  std::array<double, 3> High;
};

std::shared_ptr<const PrismGrid> Source() {
  constexpr float firstFill = .6f;
  constexpr float secondFill = .9f;
  constexpr float secondPitchX = 1.5f;
  constexpr float secondPitchZ = 2.5f;
  return std::make_shared<const PrismGrid>(PrismGrid{
      .Samples = std::make_shared<const std::vector<PrismSpan>>(
          std::vector<PrismSpan>{{.Lower = -4, .Upper = 6},
                                 {.Lower = 2, .Upper = 8},
                                 {.Lower = 0, .Upper = 0},
                                 {.Lower = 3, .Upper = 3},
                                 {.Lower = -6, .Upper = -2},
                                 {.Lower = 1, .Upper = 4}}),
      .Patches = {{.Columns = 3, .Rows = 2, .PitchX = 2, .PitchZ = 3, .FillFraction = firstFill},
                  {.Columns = 2,
                   .Rows = 3,
                   .PitchX = secondPitchX,
                   .PitchZ = secondPitchZ,
                   .FillFraction = secondFill}}});
}

WorldBox TransformedBox(const std::array<double, 3> &low,
                        const std::array<double, 3> &high,
                        const Mat4f &matrix) {
  constexpr double infinity = std::numeric_limits<double>::infinity();
  WorldBox box{.Low = {infinity, infinity, infinity}, .High = {-infinity, -infinity, -infinity}};
  for (uint32_t corner = 0; corner < 8; ++corner) {
    const Vec3f point{{static_cast<float>((corner & 1u) != 0 ? high[0] : low[0]),
                       static_cast<float>((corner & 2u) != 0 ? high[1] : low[1]),
                       static_cast<float>((corner & 4u) != 0 ? high[2] : low[2])}};
    const auto world = matrix.TransformPoint(point);
    for (size_t axis = 0; axis < 3; ++axis) {
      box.Low[axis] = std::min(box.Low[axis], static_cast<double>(world[axis]));
      box.High[axis] = std::max(box.High[axis], static_cast<double>(world[axis]));
    }
  }
  return box;
}

std::vector<WorldBox> Boxes(const PrismGrid &source, std::span<const PrismInstance> instances) {
  std::vector<WorldBox> result;
  for (const auto &instance : instances) {
    const auto &patch = source.Patches[instance.Patch];
    const double insetX = (1.0 - patch.FillFraction) * patch.PitchX * .5;
    const double insetZ = (1.0 - patch.FillFraction) * patch.PitchZ * .5;
    for (uint32_t z = 0; z < patch.Rows; ++z) {
      for (uint32_t x = 0; x < patch.Columns; ++x) {
        const auto cell = (*source.Samples)[patch.FirstSample + size_t{z} * patch.Columns + x];
        if (cell.Upper <= cell.Lower) { continue; }
        const std::array<double, 3> low{static_cast<double>(x) * patch.PitchX + insetX,
                                        cell.Lower,
                                        static_cast<double>(z) * patch.PitchZ + insetZ};
        const std::array<double, 3> high{static_cast<double>(x + 1) * patch.PitchX - insetX,
                                         cell.Upper,
                                         static_cast<double>(z + 1) * patch.PitchZ - insetZ};
        result.push_back(TransformedBox(low, high, instance.Current));
      }
    }
  }
  return result;
}

struct TestView {
  bool Below = false;
  float ShearX = 0;
  float ShearZ = 0;
  const char *Name = nullptr;
  Native::Viewport Area{.Width = kWidth, .Height = kHeight};
  bool Release = false;
};

PrismDepthView View(const TestView &view) {
  PrismDepthView result;
  result.ClipFromWorld.Column.fill(0);
  result.ClipFromWorld[0] = 2 / kWorldWidth;
  result.ClipFromWorld[4] = 2 * view.ShearX / kWorldWidth;
  result.ClipFromWorld[9] = (view.Below ? 2.f : -2.f) / kWorldDepth;
  result.ClipFromWorld[5] = result.ClipFromWorld[9] * view.ShearZ;
  result.ClipFromWorld[6] = (view.Below ? -1.f : 1.f) / (2 * kVerticalHalf);
  result.ClipFromWorld[12] = -1;
  result.ClipFromWorld[13] = view.Below ? -1 : 1;
  result.ClipFromWorld[14] = .5f;
  result.ClipFromWorld[15] = 1;
  return result;
}

struct BoxDepth {
  double Hit = 0;
  double Float32Contact = 0;
};

BoxDepth Depth(const WorldBox &box, const TestView &view, double x, double z) {
  const double yDirection = view.Below ? 1 : -1;
  const double yOrigin = -yDirection * kVerticalHalf;
  const std::array<double, 3> origin{x - view.ShearX * yOrigin, yOrigin, z - view.ShearZ * yOrigin};
  const std::array<double, 3> direction{
      -view.ShearX * yDirection, yDirection, -view.ShearZ * yDirection};
  double entry = 0;
  double leave = 2 * kVerticalHalf;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (direction[axis] == 0) {
      if (origin[axis] < box.Low[axis] || origin[axis] > box.High[axis]) { return {}; }
      continue;
    }
    const double a = (box.Low[axis] - origin[axis]) / direction[axis];
    const double b = (box.High[axis] - origin[axis]) / direction[axis];
    entry = std::max(entry, std::min(a, b));
    leave = std::min(leave, std::max(a, b));
  }
  const bool unresolved = entry <= 2 * kVerticalHalf && leave >= 0 &&
                          static_cast<float>(entry) == static_cast<float>(leave);
  return {.Hit = entry <= leave ? 1 - entry / (2 * kVerticalHalf) : 0,
          .Float32Contact = unresolved ? 1 - entry / (2 * kVerticalHalf) : 0};
}

void Compare(const ImagePixels &image, std::span<const WorldBox> boxes, const TestView &view) {
  size_t coverage = 0;
  size_t rawCoverage = 0;
  size_t contacts = 0;
  double error = 0;
  double rawError = 0;
  for (uint32_t y = 0; y < kHeight; ++y) {
    const double zFraction = (y + .5 - view.Area.Y) / view.Area.Height;
    const double worldZ = (view.Below ? 1 - zFraction : zFraction) * kWorldDepth;
    for (uint32_t x = 0; x < kWidth; ++x) {
      const double xFraction = (x + .5 - view.Area.X) / view.Area.Width;
      const double worldX = xFraction * kWorldWidth;
      double expected = 0;
      double robust = 0;
      double contact = 0;
      if (xFraction >= 0 && xFraction < 1 && zFraction >= 0 && zFraction < 1) {
        for (const auto &box : boxes) {
          const auto depth = Depth(box, view, worldX, worldZ);
          expected = std::max(expected, depth.Hit);
          contact = std::max(contact, depth.Float32Contact);
          if (depth.Float32Contact == 0) { robust = std::max(robust, depth.Hit); }
        }
      }
      float actual = 0;
      std::memcpy(
          &actual, image.Rows.data() + y * image.RowBytes + x * sizeof(float), sizeof(float));
      const bool unresolved = contact > 0 && contact >= robust;
      rawCoverage += static_cast<size_t>((actual > 0) != (expected > 0));
      rawError = std::max(rawError, std::abs(actual - expected));
      contacts += static_cast<size_t>(unresolved);
      if (!unresolved) {
        coverage += static_cast<size_t>((actual > 0) != (expected > 0));
        error = std::max(error, std::abs(actual - expected));
      }
    }
  }
  CHECK(coverage == 0, "resolved box intersections match ragged/empty raster coverage");
  CHECK(error <= 32 * std::numeric_limits<float>::epsilon(),
        "native grid traversal agrees with an independent all-box nearest intersection");
  std::println("PRISM view={} resolved_coverage={} max_resolved_depth_error={} "
               "float32_contacts={} raw_coverage={} raw_max_depth_error={}",
               view.Name,
               coverage,
               error,
               contacts,
               rawCoverage,
               rawError);
}

void Probe(Device &device, const char *diagnostic) {
  std::chrono::nanoseconds budget = std::chrono::seconds(5);
  UploadQueue uploads(device, budget);
  PrismResidency renderer;
  Require(renderer.Uses(device, &uploads));
  PrismPipelines pipelines;
  const std::array targets{ColourTarget{.Texels = Format::Rgba16Float, .Rgb = {}, .Alpha = {}}};
  Require(pipelines.ConfigureColour(device, {}, targets));
  Require(pipelines.ConfigureDepth(device));
  const auto source = Source();
  std::array<PrismInstance, 2> instances{};
  instances[0].Current[12] = instances[0].Current[14] = 1;
  instances[1].Patch = 1;
  instances[1].Current[0] = instances[1].Current[10] = -1;
  constexpr float secondX = 15;
  constexpr float secondZ = 10;
  instances[1].Current[12] = secondX;
  instances[1].Current[13] = 3;
  instances[1].Current[14] = secondZ;
  for (auto &instance : instances) { instance.Previous = instance.Current; }
  const auto boxes = Boxes(*source, instances);
  const auto id = Require(renderer.Place(source, instances, 0));
  const auto target =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  constexpr float shearX = .35f;
  constexpr float shearZ = .2f;
  const std::array views{TestView{.Name = "above"},
                         TestView{.Below = true, .Name = "below"},
                         TestView{.ShearX = shearX, .ShearZ = shearZ, .Name = "oblique"},
                         TestView{.ShearX = shearX,
                                  .ShearZ = shearZ,
                                  .Name = "tile",
                                  .Area = {.X = 8, .Y = 6, .Width = 128, .Height = 80},
                                  .Release = true}};
  for (const auto &view : views) {
    auto commands = Require(uploads.Begin());
    Require(commands->BeginRenderPass(
        {.Colours = {},
         .Depth =
             DepthAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear, .Clear = 0}}));
    Require(EncodePrismDepth(renderer,
                             *commands,
                             *pipelines.Depth(),
                             View(view),
                             view.Area,
                             {.X = static_cast<uint32_t>(view.Area.X),
                              .Y = static_cast<uint32_t>(view.Area.Y),
                              .Width = static_cast<uint32_t>(view.Area.Width),
                              .Height = static_cast<uint32_t>(view.Area.Height)}));
    Require(commands->EndRenderPass());
    if (view.Release) {
      CHECK(renderer.Release(id), "recorded raster resources survive owner release");
    }
    const auto completion = Require(SubmitUpload(device, &uploads, std::move(commands)));
    CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "native prism raster pass completes");
    const auto image = ReadImage(device, *target, {.Width = kWidth, .Height = kHeight});
    Compare(image, boxes, view);
    if (diagnostic != nullptr) {
      std::ofstream file(std::string(diagnostic) + "-" + view.Name + ".depth", std::ios::binary);
      file.write(reinterpret_cast<const char *>(image.Rows.data()),
                 static_cast<std::streamsize>(image.Rows.size()));
      CHECK(file, "explicit diagnostic readback is saved");
    }
  }
}
}

int main(int argc, char *const *argv) {
  auto device = Require(CreateDevice({.Validation = true}));
  std::println("DEVICE {}", device->Limits().Name);
  Probe(*device, argc > 1 ? argv[1] : nullptr);
  return Report();
}

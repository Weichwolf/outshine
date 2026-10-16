#include "Check.h"
#include "DepthPyramidStage.h"
#include "GpuPlacement.h"
#include "ReadbackTransfer.h"
#include "StagePipelines.h"
#include "StageSubmission.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <SDL3/SDL_stdinc.h>
#include <print>
#include <ratio>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr size_t kPlaneComponents = 24;
constexpr size_t kSourceOffset = 244;
constexpr float kOccluderDepth = 0.75f;
constexpr float kSmallSphere = 0.000001f;
constexpr float kCoarseSphere = 0.02f;
constexpr Texels kOddExtent{.WidthPx = 37, .HeightPx = 25};
constexpr Texels k720p{.WidthPx = 1280, .HeightPx = 720};
constexpr Texels k1080p{.WidthPx = 1920, .HeightPx = 1080};
constexpr std::array<Texels, 5> kEdges{{{.WidthPx = 1, .HeightPx = 1},
                                        {.WidthPx = 1, .HeightPx = 17},
                                        {.WidthPx = 17, .HeightPx = 1},
                                        {.WidthPx = 16, .HeightPx = 16},
                                        {.WidthPx = 17, .HeightPx = 15}}};
constexpr uint32_t kNoiseSeed = 282;
constexpr uint32_t kNoiseMultiplier = 1664525u;
constexpr uint32_t kNoiseIncrement = 1013904223u;
constexpr float kNoiseRange = 0.125f;
constexpr float kNoiseDivisor = 262144.0f;

struct Probe {
  uint32_t X;
  uint32_t Y;
  float Radius;
};

constexpr std::array<float, 16> kIdentity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

void Land(Device &device, std::unique_ptr<Commands> commands) {
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "real GPU commands complete within the offline wait budget");
}

std::unique_ptr<Buffer>
Upload(Device &device, Commands &commands, const void *data, uint64_t bytes) {
  auto staging = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*staging, {.Bytes = bytes}));
  std::memcpy(mapped.data(), data, bytes);
  Require(device.Unmap(*staging));
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::Storage | BufferUse::CopyDestination}));
  Require(commands.Copy(*staging, *buffer, {.Bytes = bytes}));
  return buffer;
}

struct alignas(16) CullInput {
  std::array<float, kPlaneComponents> Planes{1, 0,  0, 1, -1, 0, 0, 1, 0, 1, 0,  1,
                                             0, -1, 0, 1, 0,  0, 1, 0, 0, 0, -1, 1};
  std::array<float, 4> Shift{};
  uint32_t Jobs = 1;
  float ErrorPerMetre = 0;
  std::array<uint32_t, 2> Pad{};
  std::array<float, 16> Clip = kIdentity;
  std::array<uint32_t, 4> Wide{}, High{}, At{};
  uint32_t Occludes = 1;
  std::array<uint32_t, 3> Source{};
};

static_assert(sizeof(CullInput) == 256 && offsetof(CullInput, Source) == kSourceOffset);

uint32_t Selected(Device &device, Buffer *pyramid, PyramidShape shape, Texels extent, Probe probe) {
  const auto width = extent.WidthPx;
  const auto height = extent.HeightPx;
  const auto x = probe.X;
  const auto y = probe.Y;
  const auto radius = probe.Radius;
  const float px = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(width) - 1.0f;
  const float py = 1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
  const std::array<float, 12> spheres{
      px, py, 0.5f, radius, px, py, 0.5f, radius, 0, std::numeric_limits<float>::infinity(), 0, 0};
  constexpr std::array<uint32_t, 4> jobs{0, 0, 0, 3};
  GpuPlacement placement;
  placement.Current = kIdentity;
  constexpr std::array<uint32_t, 5> arguments{3, 1, 0, 0, 0};
  auto commands = Require(device.Begin());
  const auto bounds = Upload(device, *commands, spheres.data(), sizeof(spheres));
  const auto work = Upload(device, *commands, jobs.data(), sizeof(jobs));
  const auto model = Upload(device, *commands, &placement, sizeof(placement));
  const auto args = Upload(device, *commands, arguments.data(), sizeof(arguments));
  auto kept = Require(device.CreateBuffer(
      {.Bytes = sizeof(uint32_t), .Use = BufferUse::Storage | BufferUse::CopySource}));
  auto pipeline = Require(CreateComputePipeline(device, ComputeShaderId::SubjectCull));
  CullInput view;
  view.Wide = shape.Wide;
  view.High = shape.High;
  view.At = shape.At;
  view.Source = {width, height, 0};
  const std::array bindings{
      Storage(0, 0, *bounds),
      Storage(0, 1, *work),
      Storage(0, 2, *model),
      Storage(0, 3, *args),
      Storage(0, 4, *pyramid),
      Storage(1, 0, *kept),
      Binding{.Set = 2,
              .Slot = 0,
              .Value = Uniform{.Bytes = std::as_bytes(std::span{&view, size_t{1}})}}};
  Require(commands->Dispatch(*pipeline, bindings, {.X = 1, .Y = 1, .Z = 1}));
  Land(device, std::move(commands));
  auto readback = Require(ReadbackTransfer::Start(device, *kept, {.Bytes = sizeof(uint32_t)}));
  const auto result = Require(readback.Wait(device, std::chrono::seconds(5)));
  CHECK(result, "actual culling output is readable");
  if (!result) { Failed(); }
  uint32_t selected = ~uint32_t{0};
  std::memcpy(&selected, result->data(), sizeof(selected));
  return selected;
}

std::vector<float> DepthPixels(uint32_t width, uint32_t height, bool border) {
  std::vector<float> pixels(static_cast<size_t>(width) * height, kOccluderDepth);
  uint32_t noise = kNoiseSeed;
  for (float &pixel : pixels) {
    noise = noise * kNoiseMultiplier + kNoiseIncrement;
    pixel = kOccluderDepth - kNoiseRange + static_cast<float>(noise >> 16u) / kNoiseDivisor;
  }
  if (border) {
    for (uint32_t x = 0; x < width; ++x) { pixels[(height - 1u) * width + x] = 0; }
  } else {
    pixels[(height / 2u) * width + width - 2u] = 0;
  }
  return pixels;
}

void CheckEveryDepth(std::span<const float> source,
                     Texels extent,
                     PyramidShape shape,
                     const float *reduced) {
  bool equal = true;
  for (uint32_t level = 0; level < kPyramidLevels; ++level) {
    const uint32_t block = 2u << level;
    for (uint32_t y = 0; y < shape.High[level]; ++y) {
      for (uint32_t x = 0; x < shape.Wide[level]; ++x) {
        float farthest = 1;
        for (uint32_t down = 0; down < block; ++down) {
          const auto row = std::min(y * block + down, extent.HeightPx - 1u);
          for (uint32_t across = 0; across < block; ++across) {
            const auto col = std::min(x * block + across, extent.WidthPx - 1u);
            farthest = std::min(farthest, source[row * extent.WidthPx + col]);
          }
        }
        equal = equal && reduced[shape.At[level] + y * shape.Wide[level] + x] == farthest;
      }
    }
  }
  CHECK(equal, "every GPU pyramid texel equals the direct clamped reverse-Z block minimum");
}

void MeasureReduction(Device &device, DepthPyramidStage &stage, Texels extent) {
  if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "OUTSHINE_DEPTH_PYRAMID_BENCHMARK") ==
      nullptr) {
    return;
  }
  constexpr uint32_t repeats = 128;
  const auto start = std::chrono::steady_clock::now();
  auto commands = Require(device.Begin());
  StageSubmission submission;
  for (uint32_t at = 0; at < repeats; ++at) {
    Require(stage.Encode(
        {.Commands = commands.get(), .Submission = submission, .Area = {}, .Clip = {}}));
  }
  const auto encoded = std::chrono::steady_clock::now();
  Land(device, std::move(commands));
  const auto finished = std::chrono::steady_clock::now();
  const double host = std::chrono::duration<double, std::milli>(encoded - start).count() / repeats;
  const double landed =
      std::chrono::duration<double, std::milli>(finished - start).count() / repeats;
  std::println("BENCH depth={}x{} repeats={} host_ms={:.6f} landed_ms={:.6f} device={} "
               "gpu_timestamp=not_requested",
               extent.WidthPx,
               extent.HeightPx,
               repeats,
               host,
               landed,
               device.Limits().Name);
}

void CheckExtent(Device &device, Texels extent, bool border) {
  const auto width = extent.WidthPx;
  const auto height = extent.HeightPx;
  const auto pixels = DepthPixels(width, height, border);
  auto texture =
      Require(device.CreateTexture({.Width = width,
                                    .Height = height,
                                    .Texels = Format::R32Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  const auto bytes = pixels.size() * sizeof(float);
  auto transfer = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*transfer, {.Bytes = bytes}));
  std::memcpy(mapped.data(), pixels.data(), bytes);
  Require(device.Unmap(*transfer));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      *transfer,
      *texture,
      {.RowBytes = uint64_t{width} * sizeof(float), .Region = {.Width = width, .Height = height}}));
  Land(device, std::move(commands));
  const PyramidShape shape = PyramidOver({.WidthPx = width, .HeightPx = height});
  auto pyramid = Require(device.CreateBuffer({.Bytes = uint64_t{shape.Texels} * sizeof(float),
                                              .Use = BufferUse::Storage | BufferUse::CopySource}));
  auto sampler = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest, .MaxLod = 0}));
  DepthPyramidStage stage;
  Require(
      stage.Configure({.Device = &device, .SceneColours = {}},
                      texture.get(),
                      sampler.get(),
                      pyramid.get(),
                      {.WidthPx = static_cast<int>(width), .HeightPx = static_cast<int>(height)}));
  commands = Require(device.Begin());
  StageSubmission submission;
  Require(
      stage.Encode({.Commands = commands.get(), .Submission = submission, .Area = {}, .Clip = {}}));
  Land(device, std::move(commands));
  auto read = Require(
      ReadbackTransfer::Start(device, *pyramid, {.Bytes = uint64_t{shape.Texels} * sizeof(float)}));
  const auto output = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(output, "reduced depth is read from the actual GPU");
  if (!output) { Failed(); }
  std::vector<float> reduced(shape.Texels);
  std::memcpy(reduced.data(), output->data(), output->size());
  const auto *values = reduced.data();
  CheckEveryDepth(pixels, extent, shape, values);
  if (width >= k720p.WidthPx) { MeasureReduction(device, stage, extent); }
  for (uint32_t level = 0; level < kPyramidLevels; ++level) {
    const uint32_t block = 2u << level;
    CHECK(shape.Wide[level] == (width + block - 1u) / block &&
              shape.High[level] == (height + block - 1u) / block,
          "every source pixel belongs to a depth block at every level");
    if (border) {
      const auto lastRow = shape.At[level] + (shape.High[level] - 1u) * shape.Wide[level];
      CHECK(values[lastRow] == 0, "uncovered last source row reaches the last reduced block");
    }
  }
  if (width < 4 || height < 4) { return; }
  const uint32_t x = border ? width / 2u : width - 2u;
  const uint32_t y = border ? height - 1u : height / 2u;
  CHECK(Selected(device, pyramid.get(), shape, extent, {.X = x, .Y = y, .Radius = kSmallSphere}) ==
            3,
        "a genuine uncovered pixel cannot hide its cluster");
  if (border) {
    CHECK(Selected(
              device, pyramid.get(), shape, extent, {.X = x, .Y = y, .Radius = kCoarseSphere}) == 3,
          "coarser reduction also preserves uncovered edge coverage");
  }
  CHECK(Selected(device,
                 pyramid.get(),
                 shape,
                 extent,
                 {.X = width / 2u, .Y = height / 2u, .Radius = kSmallSphere}) == 0,
        "fully covered interior still rejects the hidden cluster");
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  for (const auto extent : kEdges) { CheckExtent(*device, extent, true); }
  CheckExtent(*device, kOddExtent, true);
  CheckExtent(*device, kOddExtent, false);
  CheckExtent(*device, k720p, true);
  CheckExtent(*device, k1080p, true);
  return Report();
}

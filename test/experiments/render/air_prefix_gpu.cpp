#include "Atmosphere.h"
#include "render/device/native/Device.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <ios>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <utility>
#include <system_error>
#include <type_traits>
#include <vector>

namespace {
using namespace outshine;
namespace gpu = outshine::Render::Native;
constexpr uint32_t kGroupWidth = 64;
constexpr size_t kMaximumFileBytes = size_t{128} * 1024 * 1024;
using uint = uint32_t;
#include "AirRayLayout.h"
#include "AirPrefixProbeLayout.h"
constexpr uint32_t kRayBytes = sizeof(ProbeRay);
constexpr uint32_t kSampleBytes = sizeof(ProbeSample);
constexpr uint32_t kPixelChannels = 4;
static_assert(std::endian::native == std::endian::little);
static_assert(std::is_trivially_copyable_v<ProbeRay> && sizeof(ProbeRay) == 32);
static_assert(sizeof(AirRay) == 5 * sizeof(float) &&
              offsetof(ProbeRay, QueryBegin) == sizeof(AirRay));
static_assert(offsetof(ProbeRay, QueryEnd) == 6 * sizeof(float) &&
              offsetof(ProbeRay, Pad) == 7 * sizeof(float));
static_assert(std::is_trivially_copyable_v<ProbeSample> && sizeof(ProbeSample) == 32);
static_assert(offsetof(ProbeSample, ThroughR) == 12 &&
              offsetof(ProbeSample, SourceEvaluations) == 6 * sizeof(float) &&
              offsetof(ProbeSample, Pad) == 7 * sizeof(float));

std::vector<std::byte> Read(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto bytes = static_cast<std::streamoff>(file.tellg());
  if (!file || bytes <= 0 || std::cmp_greater(bytes, kMaximumFileBytes)) { return {}; }
  std::vector<std::byte> data(static_cast<size_t>(bytes));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(data.data()), bytes)) { return {}; }
  return data;
}

struct TextureExtent {
  uint32_t WidthPx;
  uint32_t HeightPx;
};

static_assert(sizeof(TextureExtent) == 2 * sizeof(uint32_t));

struct Data {
  std::array<std::vector<std::byte>, 5> Sources;
  std::array<TextureExtent, 2> Extents{};
  std::array<gpu::Format, 2> Formats{};
  std::array<uint32_t, 2> PixelBytes{};
  uint32_t RayCount = 0;
  uint32_t QueryCount = 0;
  Medium Air{};

  [[nodiscard]] bool QueriesValid() const {
    const auto &rays = Sources[0];
    const auto &queries = Sources[1];
    uint32_t previousEnd = 0;
    for (size_t offset = 0; offset < rays.size(); offset += kRayBytes) {
      ProbeRay ray{};
      std::memcpy(&ray, rays.data() + offset, sizeof(ray));
      if (ray.QueryBegin != previousEnd || ray.QueryBegin > ray.QueryEnd ||
          ray.QueryEnd > QueryCount || !std::isfinite(ray.Ray.RadiusKm) ||
          ray.Ray.RadiusKm < Air.BottomRadiusKm || ray.Ray.RadiusKm > Air.TopRadiusKm ||
          !std::isfinite(ray.Ray.DistanceKm) || ray.Ray.DistanceKm < 0.0f ||
          !(std::fabs(ray.Ray.CosView) <= 1.0f) || !(std::fabs(ray.Ray.CosSun) <= 1.0f) ||
          !(std::fabs(ray.Ray.ViewSun) <= 1.0f)) {
        return false;
      }
      float previousKm = 0.0f;
      for (uint32_t index = ray.QueryBegin; index < ray.QueryEnd; ++index) {
        float distanceKm = 0.0f;
        std::memcpy(&distanceKm, queries.data() + size_t{index} * sizeof(float), sizeof(float));
        if (!std::isfinite(distanceKm) || distanceKm < previousKm ||
            distanceKm > ray.Ray.DistanceKm) {
          return false;
        }
        previousKm = distanceKm;
      }
      previousEnd = ray.QueryEnd;
    }
    return previousEnd == QueryCount;
  }

  bool Load(char **paths) {
    for (size_t index = 0; index < Sources.size(); ++index) { Sources[index] = Read(paths[index]); }
    for (size_t index = 0; index < Extents.size(); ++index) {
      auto &pixels = Sources[index + 2];
      if (pixels.size() < sizeof(TextureExtent)) { return false; }
      std::memcpy(&Extents[index], pixels.data(), sizeof(TextureExtent));
      const auto extent = Extents[index];
      constexpr uint32_t kMaximumDimension = 2048;
      if (extent.WidthPx == 0 || extent.HeightPx == 0 || extent.WidthPx > kMaximumDimension ||
          extent.HeightPx > kMaximumDimension) {
        return false;
      }
      const auto channels = size_t{extent.WidthPx} * extent.HeightPx * kPixelChannels;
      const auto payload = pixels.size() - sizeof(TextureExtent);
      if (payload == channels * sizeof(uint16_t)) {
        Formats[index] = gpu::Format::Rgba16Float;
        PixelBytes[index] = kPixelChannels * sizeof(uint16_t);
      } else if (payload == channels * sizeof(float)) {
        Formats[index] = gpu::Format::Rgba32Float;
        PixelBytes[index] = kPixelChannels * sizeof(float);
      } else {
        return false;
      }
      pixels.erase(pixels.begin(), pixels.begin() + sizeof(TextureExtent));
    }
    const auto &rays = Sources[0];
    const auto &queries = Sources[1];
    if (rays.empty() || rays.size() % kRayBytes != 0 || queries.empty() ||
        queries.size() % sizeof(float) != 0 || Sources[4].size() != sizeof(Medium)) {
      return false;
    }
    std::memcpy(&Air, Sources[4].data(), sizeof(Air));
    RayCount = static_cast<uint32_t>(rays.size() / kRayBytes);
    QueryCount = static_cast<uint32_t>(queries.size() / sizeof(float));
    return QueriesValid();
  }
};

template <typename Value> bool Keep(gpu::Result<Value> result, Value &destination) {
  if (!result) {
    std::println(stderr, "native GPU: {}", result.error().Message);
    return false;
  }
  destination = std::move(*result);
  return true;
}

bool Check(const gpu::Result<void> &result) {
  if (!result) { std::println(stderr, "native GPU: {}", result.error().Message); }
  return result.has_value();
}

bool Fill(gpu::Device &device, gpu::Buffer &buffer, std::span<const std::byte> bytes) {
  auto mapped = device.MapWrite(buffer, {.Bytes = bytes.size()});
  if (!mapped) {
    std::println(stderr, "native GPU map: {}", mapped.error().Message);
    return false;
  }
  std::ranges::copy(bytes, mapped->begin());
  return Check(device.Unmap(buffer));
}

struct Inputs {
  std::unique_ptr<gpu::Buffer> Rays, Queries, Output, Readback;
  std::unique_ptr<gpu::Texture> Veil, Multi;
  std::unique_ptr<gpu::Sampler> Linear;
  std::array<std::unique_ptr<gpu::Buffer>, 4> Transfers;

  bool Create(gpu::Device &device, const Data &data, bool generateVeil) {
    const uint64_t outputBytes = uint64_t{data.QueryCount} * kSampleBytes;
    constexpr auto kReadUse = gpu::BufferUse::CopyDestination | gpu::BufferUse::Storage;
    constexpr auto kOutputUse = gpu::BufferUse::Storage | gpu::BufferUse::CopySource;
    if (!Keep(device.CreateBuffer({.Bytes = data.Sources[0].size(), .Use = kReadUse}, "AirRays"),
              Rays) ||
        !Keep(device.CreateBuffer({.Bytes = data.Sources[1].size(), .Use = kReadUse}, "AirQueries"),
              Queries) ||
        !Keep(device.CreateBuffer({.Bytes = outputBytes, .Use = kOutputUse}, "AirResults"),
              Output) ||
        !Keep(device.CreateBuffer({.Bytes = outputBytes,
                                   .Use = gpu::BufferUse::CopyDestination,
                                   .Placement = gpu::Memory::Readback},
                                  "AirReadback"),
              Readback) ||
        !Keep(device.CreateSampler({}), Linear)) {
      return false;
    }
    constexpr auto kTextureUse = gpu::TextureUse::CopyDestination | gpu::TextureUse::Sampled;
    const std::array<std::unique_ptr<gpu::Texture> *, 2> textures = {&Veil, &Multi};
    for (size_t index = 0; index < textures.size(); ++index) {
      const auto use =
          index == 0 && generateVeil
              ? gpu::TextureUse::Storage | gpu::TextureUse::CopySource | gpu::TextureUse::Sampled
              : kTextureUse;
      if (!device.Supports(data.Formats[index], use, true)) {
        std::println(stderr,
                     "native GPU: filtered LUT with {} byte texels is unsupported",
                     data.PixelBytes[index]);
        return false;
      }
      if (!Keep(device.CreateTexture({.Width = data.Extents[index].WidthPx,
                                      .Height = data.Extents[index].HeightPx,
                                      .Texels = data.Formats[index],
                                      .Use = use}),
                *textures[index])) {
        return false;
      }
    }
    for (size_t index = 0; index < Transfers.size(); ++index) {
      if (generateVeil && index == 2) { continue; }
      if (!Keep(device.CreateBuffer({.Bytes = data.Sources[index].size(),
                                     .Use = gpu::BufferUse::CopySource,
                                     .Placement = gpu::Memory::Upload}),
                Transfers[index]) ||
          !Fill(device, *Transfers[index], data.Sources[index])) {
        return false;
      }
    }
    return true;
  }

  bool Copy(gpu::Commands &commands, const Data &data) const {
    const std::array<gpu::Buffer *, 2> buffers = {Rays.get(), Queries.get()};
    for (size_t index = 0; index < buffers.size(); ++index) {
      if (!Check(commands.Copy(
              *Transfers[index], *buffers[index], {.Bytes = data.Sources[index].size()}))) {
        return false;
      }
    }
    const std::array<gpu::Texture *, 2> textures = {Veil.get(), Multi.get()};
    for (size_t index = 0; index < textures.size(); ++index) {
      if (!Transfers[index + buffers.size()]) { continue; }
      if (!Check(commands.Copy(
              *Transfers[index + buffers.size()],
              *textures[index],
              {.RowBytes = uint64_t{data.Extents[index].WidthPx} * data.PixelBytes[index],
               .Region = {.Width = data.Extents[index].WidthPx,
                          .Height = data.Extents[index].HeightPx}}))) {
        return false;
      }
    }
    return true;
  }
};

bool ReadPipeline(gpu::Device &device,
                  const char *path,
                  std::unique_ptr<gpu::ComputePipeline> &pipeline) {
  const auto bytes = Read(path);
  if (bytes.empty() || bytes.size() % sizeof(uint32_t) != 0) { return false; }
  std::vector<uint32_t> spirv(bytes.size() / sizeof(uint32_t));
  std::memcpy(spirv.data(), bytes.data(), bytes.size());
  return Keep(device.CreateComputePipeline(spirv), pipeline);
}

struct VeilPass {
  std::unique_ptr<gpu::ComputePipeline> Pipeline;
  std::unique_ptr<gpu::Buffer> Readback;

  bool Create(gpu::Device &device, const char *shader, const Data &data) {
    if (data.Formats[0] != gpu::Format::Rgba16Float) {
      std::println(stderr, "production Veil generation requires an explicit RGBA16F extent");
      return false;
    }
    if (!ReadPipeline(device, shader, Pipeline)) { return false; }
    const auto &layout = Pipeline->Layout();
    if (layout.LocalSize != std::array<uint32_t, 3>{8, 8, 1} || layout.Bindings.size() != 2 ||
        !std::ranges::any_of(layout.Bindings,
                             [](const auto &binding) {
                               return binding.Set == 1 && binding.Slot == 0 &&
                                      binding.Kind == gpu::BindingKind::StorageTexture &&
                                      binding.Usage == gpu::Access::Write;
                             }) ||
        !std::ranges::any_of(layout.Bindings, [](const auto &binding) {
          return binding.Set == 2 && binding.Slot == 0 &&
                 binding.Kind == gpu::BindingKind::Uniform && binding.Usage == gpu::Access::Read &&
                 binding.MinimumBytes == sizeof(Medium);
        })) {
      std::println(stderr, "unexpected production Veil layout; expected image and Medium80");
      return false;
    }
    const auto extent = data.Extents[0];
    return Keep(device.CreateBuffer(
                    {.Bytes = uint64_t{extent.WidthPx} * extent.HeightPx * data.PixelBytes[0],
                     .Use = gpu::BufferUse::CopyDestination,
                     .Placement = gpu::Memory::Readback},
                    "AirVeilReadback"),
                Readback);
  }

  bool Record(gpu::Commands &commands, const Inputs &inputs, const Data &data) const {
    const std::array bindings{
        gpu::Binding{.Set = 1,
                     .Slot = 0,
                     .Value = gpu::StorageTexture{.Image = {.Image = inputs.Veil.get()}}},
        gpu::Binding{.Set = 2,
                     .Slot = 0,
                     .Value = gpu::Uniform{.Bytes = std::as_bytes(std::span(&data.Air, 1))}}};
    const auto extent = data.Extents[0];
    return Check(commands.Dispatch(
        *Pipeline, bindings, {.X = (extent.WidthPx + 7) / 8, .Y = (extent.HeightPx + 7) / 8}));
  }

  bool Copy(gpu::Commands &commands, const Inputs &inputs, const Data &data) const {
    const auto extent = data.Extents[0];
    return Check(commands.Copy(*inputs.Veil,
                               *Readback,
                               {.RowBytes = uint64_t{extent.WidthPx} * data.PixelBytes[0],
                                .Region = {.Width = extent.WidthPx, .Height = extent.HeightPx}}));
  }

  bool Save(gpu::Device &device, const Data &data, const char *output) const {
    const auto extent = data.Extents[0];
    const uint64_t bytes = uint64_t{extent.WidthPx} * extent.HeightPx * data.PixelBytes[0];
    const auto mapped = device.MapRead(*Readback, {.Bytes = bytes});
    if (!mapped) {
      std::println(stderr, "native Veil readback: {}", mapped.error().Message);
      return false;
    }
    std::ofstream file(std::string(output) + ".veil", std::ios::binary);
    file.write(reinterpret_cast<const char *>(&extent), sizeof(extent));
    file.write(reinterpret_cast<const char *>(mapped->data()), static_cast<std::streamsize>(bytes));
    file.flush();
    const bool unmapped = Check(device.Unmap(*Readback));
    return file && unmapped;
  }
};

bool LayoutValid(const gpu::ComputeLayout &layout) {
  constexpr std::array<gpu::ShaderBinding, 6> expected = {{
      {.Set = 0,
       .Slot = 0,
       .Kind = gpu::BindingKind::SampledTexture,
       .Usage = gpu::Access::Read,
       .MinimumBytes = 0},
      {.Set = 0,
       .Slot = 1,
       .Kind = gpu::BindingKind::SampledTexture,
       .Usage = gpu::Access::Read,
       .MinimumBytes = 0},
      {.Set = 0,
       .Slot = 2,
       .Kind = gpu::BindingKind::StorageBuffer,
       .Usage = gpu::Access::Read,
       .MinimumBytes = 0},
      {.Set = 0,
       .Slot = 3,
       .Kind = gpu::BindingKind::StorageBuffer,
       .Usage = gpu::Access::Read,
       .MinimumBytes = 0},
      {.Set = 1,
       .Slot = 0,
       .Kind = gpu::BindingKind::StorageBuffer,
       .Usage = gpu::Access::Write,
       .MinimumBytes = 0},
      {.Set = 2,
       .Slot = 0,
       .Kind = gpu::BindingKind::Uniform,
       .Usage = gpu::Access::Read,
       .MinimumBytes = sizeof(Medium)},
  }};
  if (layout.LocalSize != std::array<uint32_t, 3>{kGroupWidth, 1, 1} ||
      layout.Bindings.size() != expected.size()) {
    return false;
  }
  return std::ranges::all_of(expected, [&](const auto &need) {
    return std::ranges::any_of(layout.Bindings, [&](const auto &actual) {
      return actual.Set == need.Set && actual.Slot == need.Slot && actual.Kind == need.Kind &&
             actual.Usage == need.Usage &&
             (need.Kind != gpu::BindingKind::Uniform || actual.MinimumBytes == need.MinimumBytes);
    });
  });
}

bool Dispatch(gpu::Commands &commands,
              const gpu::ComputePipeline &pipeline,
              const Inputs &inputs,
              const Data &data) {
  const std::array<gpu::Binding, 6> bindings = {{
      {.Set = 0,
       .Slot = 0,
       .Value = gpu::SampledTexture{.Image = {.Image = inputs.Veil.get()},
                                    .Sampling = inputs.Linear.get()}},
      {.Set = 0,
       .Slot = 1,
       .Value = gpu::SampledTexture{.Image = {.Image = inputs.Multi.get()},
                                    .Sampling = inputs.Linear.get()}},
      {.Set = 0,
       .Slot = 2,
       .Value = gpu::StorageBuffer{.Data = inputs.Rays.get(),
                                   .Range = {.Bytes = data.Sources[0].size()}}},
      {.Set = 0,
       .Slot = 3,
       .Value = gpu::StorageBuffer{.Data = inputs.Queries.get(),
                                   .Range = {.Bytes = data.Sources[1].size()}}},
      {.Set = 1,
       .Slot = 0,
       .Value = gpu::StorageBuffer{.Data = inputs.Output.get(),
                                   .Range = {.Bytes = uint64_t{data.QueryCount} * kSampleBytes}}},
      {.Set = 2, .Slot = 0, .Value = gpu::Uniform{.Bytes = std::as_bytes(std::span(&data.Air, 1))}},
  }};
  return Check(commands.Dispatch(
      pipeline, bindings, {.X = (data.RayCount + kGroupWidth - 1u) / kGroupWidth}));
}

bool Complete(gpu::Device &device, gpu::Completion completion) {
  constexpr auto kWaitBudget = std::chrono::seconds(30);
  const auto deadline = std::chrono::steady_clock::now() + kWaitBudget;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto status = device.Wait(completion, std::chrono::milliseconds(100));
    if (!status) {
      std::println(stderr, "native GPU wait: {}", status.error().Message);
      return false;
    }
    if (*status == gpu::CompletionStatus::Ready) { return true; }
  }
  std::println(stderr, "offline probe exceeded its 30 second completion budget");
  return false;
}

bool Submit(gpu::Device &device, std::unique_ptr<gpu::Commands> commands) {
  const auto submitted = device.Submit(std::move(commands));
  if (!submitted) {
    std::println(stderr, "native GPU submit: {}", submitted.error().Message);
    return false;
  }
  return Complete(device, *submitted);
}

bool ResidentRuns(gpu::Device &device,
                  const gpu::ComputePipeline &pipeline,
                  const Inputs &inputs,
                  const Data &data,
                  uint32_t repeats) {
  std::vector<double> times;
  for (uint32_t index = 0; index < repeats; ++index) {
    std::unique_ptr<gpu::Commands> commands;
    if (!Keep(device.Begin(), commands)) { return false; }
    const auto start = std::chrono::steady_clock::now();
    if (!Dispatch(*commands, pipeline, inputs, data) || !Submit(device, std::move(commands))) {
      return false;
    }
    const auto elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
    times.push_back(elapsed.count());
  }
  if (!times.empty()) {
    std::ranges::sort(times);
    constexpr size_t kP95Numerator = 19;
    constexpr size_t kPercentileDenominator = 20;
    std::println("resident_dispatch_submit_wait_wall_ms samples={} p50={} p95={} maximum={}; "
                 "CPU/GPU queue wall, not GPU timestamps or scene frames",
                 times.size(),
                 times[times.size() / 2],
                 times[times.size() * kP95Numerator / kPercentileDenominator],
                 times.back());
  }
  return true;
}

int Run(gpu::Device &device,
        const char *shader,
        const Data &data,
        const char *output,
        uint32_t repeats,
        const char *veilShader) {
  std::unique_ptr<gpu::ComputePipeline> pipeline;
  if (!ReadPipeline(device, shader, pipeline)) { return 2; }
  if (!LayoutValid(pipeline->Layout())) {
    std::println(stderr, "unexpected SPIR-V layout; expected AirPrefix six bindings and Medium80");
    return 2;
  }
  Inputs inputs;
  VeilPass veil;
  const bool generateVeil = veilShader != nullptr;
  if (!inputs.Create(device, data, generateVeil) ||
      (generateVeil && !veil.Create(device, veilShader, data))) {
    return 2;
  }
  std::unique_ptr<gpu::Commands> commands;
  if (!Keep(device.Begin(), commands)) { return 2; }
  const auto start = std::chrono::steady_clock::now();
  const uint64_t bytes = uint64_t{data.QueryCount} * kSampleBytes;
  if (!inputs.Copy(*commands, data) || (generateVeil && !veil.Record(*commands, inputs, data)) ||
      !Dispatch(*commands, *pipeline, inputs, data) ||
      !Check(commands->Copy(*inputs.Output, *inputs.Readback, {.Bytes = bytes})) ||
      (generateVeil && !veil.Copy(*commands, inputs, data)) ||
      !Submit(device, std::move(commands))) {
    return 2;
  }
  auto mapped = device.MapRead(*inputs.Readback, {.Bytes = bytes});
  if (!mapped) {
    std::println(stderr, "native GPU readback: {}", mapped.error().Message);
    return 2;
  }
  const auto wall =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
  std::ofstream file(output, std::ios::binary);
  file.write(reinterpret_cast<const char *>(mapped->data()), static_cast<std::streamsize>(bytes));
  file.flush();
  const bool unmapped = Check(device.Unmap(*inputs.Readback));
  if (!file || !unmapped) { return 2; }
  if (generateVeil && !veil.Save(device, data, output)) { return 2; }
  std::println("device={} rays={} queries={} submit_copy_compute_readback_wall_ms={}",
               device.Limits().Name,
               data.RayCount,
               data.QueryCount,
               wall.count());
  return ResidentRuns(device, *pipeline, inputs, data, repeats) ? 0 : 2;
}
}

int main(int argc, char **argv) {
  if (argc < 8 || argc > 10) {
    std::println(stderr,
                 "usage: air-prefix-gpu kernel.spv rays.bin queries.bin veil.bin multi.bin "
                 "medium.bin output.bin [resident-repeats [veil-kernel.spv]]");
    return 2;
  }
  uint32_t repeats = 0;
  if (argc >= 9) {
    constexpr uint32_t kMaximumRepeats = 120;
    const char *last = argv[8] + std::strlen(argv[8]);
    const auto parsed = std::from_chars(argv[8], last, repeats);
    if (parsed.ec != std::errc{} || parsed.ptr != last || repeats > kMaximumRepeats) { return 2; }
  }
  Data data;
  if (!data.Load(argv + 2)) {
    std::println(stderr, "invalid fixture extents or query spans");
    return 2;
  }
  std::unique_ptr<gpu::Device> device;
  if (!Keep(gpu::CreateDevice({.Validation = true}), device)) { return 2; }
  return Run(*device, argv[1], data, argv[7], repeats, argc == 10 ? argv[9] : nullptr);
}

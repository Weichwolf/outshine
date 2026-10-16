#include "NativeGpuCheck.h"
#include "Atmosphere.h"
#include "Check.h"
#include "base/math/RenderFrame.h"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using uint = uint32_t;
#include "AirRayLayout.h"
#include "AirPrefixProbeLayout.h"

struct Grid {
  uint32_t Width, Height, FieldWidth, FieldHeight;
};

struct FieldState {
  float Cover, EastM, NorthM;
};

struct alignas(16) PackedFieldState {
  float Cover, EastM, ZM, Pad;
};

static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(Grid) == 16 && offsetof(Grid, FieldWidth) == 8);
static_assert(sizeof(PackedFieldState) == 16 && alignof(PackedFieldState) == 16 &&
              offsetof(PackedFieldState, Cover) == 0 && offsetof(PackedFieldState, EastM) == 4 &&
              offsetof(PackedFieldState, ZM) == 8 && offsetof(PackedFieldState, Pad) == 12);
constexpr size_t kMediumBytes = 80;
constexpr uint32_t kGroupWidth = 64;
constexpr float kHomogeneousScaleHeightKm = 1e19f;
static_assert(sizeof(Medium) == kMediumBytes && alignof(Medium) == 16 && sizeof(ProbeRay) == 32 &&
              sizeof(ProbeSample) == 32);
static_assert(sizeof(AirRay) == 5 * sizeof(float) &&
              offsetof(ProbeRay, QueryBegin) == sizeof(AirRay) &&
              offsetof(ProbeRay, QueryEnd) == sizeof(AirRay) + sizeof(uint32_t));
static_assert(offsetof(ProbeSample, RadianceR) == 0 &&
              offsetof(ProbeSample, ThroughR) == 3 * sizeof(float) &&
              offsetof(ProbeSample, SourceEvaluations) == 6 * sizeof(float));
static_assert(std::is_trivially_copyable_v<Medium> && std::is_trivially_copyable_v<ProbeRay>);
constexpr std::array<FieldState, 4> kStates = {
    {{.Cover = 0.65f, .EastM = 256.34352f, .NorthM = 148.0f},
     {.Cover = 0, .EastM = 256.34352f, .NorthM = 148.0f},
     {.Cover = 0.65f, .EastM = 0, .NorthM = 0},
     {.Cover = 0.65f, .EastM = 256.34352f, .NorthM = 148.0f}}};
constexpr double kNanosecondsPerMillisecond = 1'000'000.0;

constexpr PackedFieldState Pack(const FieldState &state) {
  const auto zM = static_cast<float>(RenderFrame::ZOfNorth(state.NorthM));
  return {.Cover = state.Cover, .EastM = state.EastM, .ZM = zM == 0 ? 0.0f : zM, .Pad = 0};
}

std::vector<uint32_t> Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto bytes = static_cast<std::streamoff>(file.tellg());
  constexpr std::streamoff kMaximum = 128L * 1024 * 1024;
  CHECK(bytes > 0 && bytes <= kMaximum && bytes % sizeof(uint32_t) == 0,
        "fixture has a bounded word-aligned size");
  if (bytes <= 0 || bytes > kMaximum || bytes % sizeof(uint32_t) != 0) { Failed(); }
  std::vector<uint32_t> result(static_cast<size_t>(bytes) / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(result.data()), bytes);
  CHECK(file, "fixture is completely readable");
  if (!file) { Failed(); }
  return result;
}

template <typename Value> Value Object(const std::vector<uint32_t> &words) {
  CHECK(words.size() * sizeof(uint32_t) == sizeof(Value), "fixture object size matches");
  if (words.size() * sizeof(uint32_t) != sizeof(Value)) { Failed(); }
  Value result{};
  std::memcpy(&result, words.data(), sizeof(result));
  return result;
}

void Complete(Device &device, Completion completion) {
  const auto status = Require(device.Wait(completion, std::chrono::seconds(30)));
  CHECK(status == CompletionStatus::Ready, "bounded offline GPU wait completes");
  if (status != CompletionStatus::Ready) { Failed(); }
}

struct RayInputs {
  std::unique_ptr<Buffer> Rays, Queries;
  uint32_t Count = 0;
};

RayInputs ReadRays(Device &device,
                   Commands &commands,
                   const std::filesystem::path &folder,
                   uint64_t expected,
                   const Medium &medium) {
  const auto rays = Read(folder / "rays.bin");
  const auto queries = Read(folder / "queries.bin");
  const bool sized = rays.size() == expected * 8 && queries.size() == expected;
  CHECK(sized, "one ray and one fixed-depth query per image/field point");
  if (!sized) { Failed(); }
  for (uint32_t index = 0; index < queries.size(); ++index) {
    ProbeRay ray{};
    std::memcpy(&ray, rays.data() + size_t{index} * 8, sizeof(ray));
    const bool valid = ray.QueryBegin == index && ray.QueryEnd == index + 1 &&
                       std::isfinite(ray.Ray.RadiusKm) &&
                       ray.Ray.RadiusKm >= medium.BottomRadiusKm &&
                       std::fabs(ray.Ray.CosView) <= 1 && std::fabs(ray.Ray.CosSun) <= 1 &&
                       std::fabs(ray.Ray.ViewSun) <= 1 && std::isfinite(ray.Ray.DistanceKm) &&
                       ray.Ray.DistanceKm >= 10 && queries[index] == std::bit_cast<uint32_t>(10.0f);
    if (!valid) {
      CHECK(false, "invalid ray or query outside the declared fixed-depth control");
      Failed();
    }
  }
  return {.Rays = Upload(device, commands, rays),
          .Queries = Upload(device, commands, queries),
          .Count = static_cast<uint32_t>(expected)};
}

std::unique_ptr<Texture>
ReadLut(Device &device, Commands &commands, const std::filesystem::path &path) {
  const auto words = Read(path);
  const bool valid = words.size() >= 2 && words[0] > 0 && words[0] <= 2048 && words[1] > 0 &&
                     words[1] <= 2048 && words.size() == 2 + uint64_t{words[0]} * words[1] * 2;
  CHECK(valid, "LUT has an explicit bounded RGBA16F extent and payload");
  if (!valid) { Failed(); }
  const uint64_t bytes = (words.size() - 2) * sizeof(uint32_t);
  auto texture =
      Require(device.CreateTexture({.Width = words[0],
                                    .Height = words[1],
                                    .Texels = Format::Rgba16Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes}));
  std::memcpy(mapped.data(), words.data() + 2, static_cast<size_t>(bytes));
  Require(device.Unmap(*upload));
  Require(commands.Copy(
      *upload,
      *texture,
      {.RowBytes = uint64_t{words[0]} * 8, .Region = {.Width = words[0], .Height = words[1]}}));
  return texture;
}

struct Resident {
  Grid Extent{};
  Medium Air{};
  RayInputs Coarse, Image;
  std::unique_ptr<Texture> Veil, Multi, Field;
  std::unique_ptr<Sampler> Linear;
  std::unique_ptr<Buffer> Output, Readback, FieldReadback, Trace;
  std::unique_ptr<ComputePipeline> Producer, Reconstruction, Normalization;
};

enum class Pass { Reconstruction, Fused, Trace, Normalization };

void Layout(const ComputeLayout &layout, Pass pass) {
  CHECK((layout.LocalSize == std::array<uint32_t, 3>{64, 1, 1}), "expected compute workgroup");
  std::array<ShaderBinding, 7> expected = {
      {{.Set = 0, .Slot = 0, .Kind = BindingKind::SampledTexture},
       {.Set = 0, .Slot = 1, .Kind = BindingKind::SampledTexture},
       {.Set = 0, .Slot = 2, .Kind = BindingKind::StorageBuffer},
       {.Set = 0, .Slot = 3, .Kind = BindingKind::StorageBuffer},
       {.Set = 1, .Slot = 0, .Kind = BindingKind::StorageBuffer, .Usage = Access::Write},
       {.Set = 2, .Slot = 0, .Kind = BindingKind::Uniform, .MinimumBytes = sizeof(Medium)},
       {.Set = 0, .Slot = 4, .Kind = BindingKind::SampledTexture}}};
  if (pass != Pass::Reconstruction) {
    expected[4].Kind =
        pass == Pass::Trace ? BindingKind::StorageBuffer : BindingKind::StorageTexture;
    expected[6] = pass == Pass::Normalization
                      ? ShaderBinding{.Set = 0, .Slot = 4, .Kind = BindingKind::StorageBuffer}
                      : ShaderBinding{.Set = 2,
                                      .Slot = 1,
                                      .Kind = BindingKind::Uniform,
                                      .MinimumBytes = sizeof(PackedFieldState)};
  }
  const bool valid = layout.LocalSize == std::array<uint32_t, 3>{64, 1, 1} &&
                     layout.Bindings.size() == expected.size() &&
                     std::ranges::all_of(expected, [&](const auto &need) {
                       return std::ranges::any_of(layout.Bindings, [&](const auto &actual) {
                         return actual.Set == need.Set && actual.Slot == need.Slot &&
                                actual.Kind == need.Kind && actual.Usage == need.Usage &&
                                actual.MinimumBytes == need.MinimumBytes;
                       });
                     });
  CHECK(valid, "actual SPIR-V bindings match the private field producer/consumer contract");
  if (!valid) { Failed(); }
}

Resident Prepare(Device &device, char **paths, const char *normalizer) {
  Resident data;
  const std::filesystem::path full(paths[2]);
  const std::filesystem::path coarse(paths[3]);
  data.Extent = Object<Grid>(Read(coarse / "gpu-grid.bin"));
  data.Air = Object<Medium>(Read(full / "medium.bin"));
  const auto grid = data.Extent;
  bool valid = grid.Width >= 2 && grid.Height >= 2 && grid.Width <= 4096 && grid.Height <= 4096 &&
               grid.FieldWidth >= 2 && grid.FieldHeight >= 2 && grid.FieldWidth <= grid.Width &&
               grid.FieldHeight <= grid.Height &&
               data.Air.RayleighScaleHeightKm >= kHomogeneousScaleHeightKm &&
               data.Air.MieScaleHeightKm >= kHomogeneousScaleHeightKm;
  for (int channel = 0; channel < 3; ++channel) {
    const float extinction =
        data.Air.RayleighScatteringPerKm[channel] + data.Air.MieExtinctionPerKm;
    valid = valid && data.Air.OzoneAbsorptionPerKm[channel] == 0 && std::isfinite(extinction) &&
            extinction > 0;
  }
  CHECK(valid, "bounded grid and homogeneous-air control");
  if (!valid) { Failed(); }
  data.Producer = Require(device.CreateComputePipeline(Spirv(paths[0])));
  data.Reconstruction = Require(device.CreateComputePipeline(Spirv(paths[1])));
  Layout(data.Producer->Layout(), normalizer != nullptr ? Pass::Trace : Pass::Fused);
  Layout(data.Reconstruction->Layout(), Pass::Reconstruction);
  if (normalizer != nullptr) {
    data.Normalization = Require(device.CreateComputePipeline(Spirv(normalizer)));
    Layout(data.Normalization->Layout(), Pass::Normalization);
    data.Trace = Require(device.CreateBuffer(
        {.Bytes = uint64_t{grid.FieldWidth} * grid.FieldHeight * sizeof(ProbeSample),
         .Use = BufferUse::Storage}));
  }
  auto commands = Require(device.Begin());
  data.Coarse =
      ReadRays(device, *commands, coarse, uint64_t{grid.FieldWidth} * grid.FieldHeight, data.Air);
  data.Image = ReadRays(device, *commands, full, uint64_t{grid.Width} * grid.Height, data.Air);
  data.Veil = ReadLut(device, *commands, full / "veil.bin");
  data.Multi = ReadLut(device, *commands, full / "multi.bin");
  data.Field = Require(device.CreateTexture(
      {.Width = grid.FieldWidth,
       .Height = grid.FieldHeight,
       .Texels = Format::Rgba32Float,
       .Use = TextureUse::Storage | TextureUse::Sampled | TextureUse::CopySource}));
  data.Linear = Require(device.CreateSampler({}));
  const uint64_t outputBytes = uint64_t{data.Image.Count} * sizeof(ProbeSample);
  data.Output = Require(device.CreateBuffer(
      {.Bytes = outputBytes, .Use = BufferUse::Storage | BufferUse::CopySource}));
  data.Readback = Require(device.CreateBuffer(
      {.Bytes = outputBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  data.FieldReadback =
      Require(device.CreateBuffer({.Bytes = uint64_t{grid.FieldWidth} * grid.FieldHeight * 16,
                                   .Use = BufferUse::CopyDestination,
                                   .Placement = Memory::Readback}));
  Complete(device, Require(device.Submit(std::move(commands))));
  return data;
}

std::array<Binding, 7>
Bindings(const Resident &data, const PackedFieldState &state, bool producer) {
  const auto &rays = producer ? data.Coarse : data.Image;
  std::array<Binding, 7> bindings = {
      {{.Set = 0,
        .Slot = 0,
        .Value =
            SampledTexture{.Image = {.Image = data.Veil.get()}, .Sampling = data.Linear.get()}},
       {.Set = 0,
        .Slot = 1,
        .Value =
            SampledTexture{.Image = {.Image = data.Multi.get()}, .Sampling = data.Linear.get()}},
       Storage(0, 2, *rays.Rays),
       Storage(0, 3, *rays.Queries),
       Storage(1, 0, *data.Output),
       {.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&data.Air, 1))}},
       {.Set = 0,
        .Slot = 4,
        .Value =
            SampledTexture{.Image = {.Image = data.Field.get()}, .Sampling = data.Linear.get()}}}};
  if (producer) {
    bindings[4] = data.Trace
                      ? Storage(1, 0, *data.Trace)
                      : Binding{.Set = 1,
                                .Slot = 0,
                                .Value = StorageTexture{.Image = {.Image = data.Field.get()}}};
    bindings[6] = {.Set = 2, .Slot = 1, .Value = Uniform{std::as_bytes(std::span(&state, 1))}};
  }
  return bindings;
}

void Normalize(Commands &commands, const Resident &data, const PackedFieldState &state) {
  if (!data.Normalization) { return; }
  auto bindings = Bindings(data, state, true);
  bindings[4] = {
      .Set = 1, .Slot = 0, .Value = StorageTexture{.Image = {.Image = data.Field.get()}}};
  bindings[6] = Storage(0, 4, *data.Trace);
  Require(commands.Dispatch(
      *data.Normalization, bindings, {.X = (data.Coarse.Count + kGroupWidth - 1) / kGroupWidth}));
}

void Copy(Commands &commands, const Resident &data) {
  Require(
      commands.Copy(*data.Output, *data.Readback, {.Bytes = data.Readback->Description().Bytes}));
  Require(commands.Copy(
      *data.Field,
      *data.FieldReadback,
      {.RowBytes = uint64_t{data.Extent.FieldWidth} * 16,
       .Region = {.Width = data.Extent.FieldWidth, .Height = data.Extent.FieldHeight}}));
}

double Milliseconds(uint64_t begin, uint64_t end, const DispatchTiming &timing) {
  const uint64_t mask = timing.ValidBits == 64 ? std::numeric_limits<uint64_t>::max()
                                               : (uint64_t{1} << timing.ValidBits) - 1;
  return static_cast<double>((end - begin) & mask) * timing.NanosecondsPerTick /
         kNanosecondsPerMillisecond;
}

void Timings(Device &device, Completion completion, uint32_t state, uint32_t repeat, bool staged) {
  const auto timings = Require(device.GpuTimings(completion));
  bool valid = timings.Status == CompletionStatus::Ready && !timings.Truncated &&
               timings.DispatchCount == (staged ? 3u : 2u) &&
               timings.Dispatches.size() == (staged ? 3u : 2u);
  for (const auto &row : timings.Dispatches) {
    valid = valid && row.Available && row.ValidBits > 0 && row.ValidBits <= 64 &&
            std::isfinite(row.NanosecondsPerTick) && row.NanosecondsPerTick > 0;
  }
  CHECK(valid, "both dependent dispatch timestamps are complete and available");
  if (!valid) { Failed(); }
  const auto &a = timings.Dispatches[0];
  const auto &b = timings.Dispatches.back();
  for (size_t index = 0; index < timings.Dispatches.size(); ++index) {
    const auto &row = timings.Dispatches[index];
    const bool compatible = row.Ordinal == index && row.ValidBits == a.ValidBits &&
                            row.NanosecondsPerTick == a.NanosecondsPerTick;
    CHECK(compatible, "every dependent interval uses the same GPU clock and ordinal");
    if (!compatible) { Failed(); }
    std::println("gpu_dispatch_timing state={} repeat={} ordinal={} available=true begin={} end={} "
                 "ns_per_tick={} valid_bits={} elapsed_ms={}",
                 state,
                 repeat,
                 row.Ordinal,
                 row.Begin,
                 row.End,
                 row.NanosecondsPerTick,
                 row.ValidBits,
                 Milliseconds(row.Begin, row.End, row));
  }
  CHECK(a.ValidBits == b.ValidBits && a.NanosecondsPerTick == b.NanosecondsPerTick,
        "dependent dispatches use the same documented GPU clock");
  const double normalization = staged ? Milliseconds(timings.Dispatches[1].Begin,
                                                     timings.Dispatches[1].End,
                                                     timings.Dispatches[1])
                                      : 0;
  std::println("gpu_pair state={} repeat={} producer_ms={} normalization_ms={} "
               "reconstruction_ms={} pair_span_ms={}",
               state,
               repeat,
               Milliseconds(a.Begin, a.End, a),
               normalization,
               Milliseconds(b.Begin, b.End, b),
               Milliseconds(a.Begin, b.End, a));
}

void MemoryModel(const Resident &data) {
  const uint64_t inputs =
      data.Coarse.Rays->Description().Bytes + data.Coarse.Queries->Description().Bytes +
      data.Image.Rays->Description().Bytes + data.Image.Queries->Description().Bytes;
  const uint64_t lutBytes =
      uint64_t{data.Veil->Description().Width} * data.Veil->Description().Height * 8 +
      uint64_t{data.Multi->Description().Width} * data.Multi->Description().Height * 8;
  const uint64_t trace = data.Trace ? data.Trace->Description().Bytes : 0;
  std::println("logical_memory buffer_device_bytes={} buffer_readback_bytes={} "
               "texture_payload_bytes={} initial_upload_payload_bytes={} "
               "capture_copy_payload_bytes={} uniform_payload_bytes_per_submission={}",
               inputs + data.Output->Description().Bytes + trace,
               data.Readback->Description().Bytes + data.FieldReadback->Description().Bytes,
               data.FieldReadback->Description().Bytes + lutBytes,
               inputs + lutBytes,
               (data.Readback->Description().Bytes + data.FieldReadback->Description().Bytes) *
                   kStates.size(),
               (data.Normalization ? 3 : 2) * sizeof(Medium) + sizeof(PackedFieldState));
}

void Save(Device &device, Buffer &buffer, const std::filesystem::path &path) {
  const auto bytes = Require(device.MapRead(buffer, {.Bytes = buffer.Description().Bytes}));
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  file.flush();
  CHECK(file, "complete diagnostic artifact was saved");
  Require(device.Unmap(buffer));
  if (!file) { Failed(); }
}

void Run(Device &device, const Resident &data, const char *output, uint32_t repeats) {
  for (uint32_t state = 0; state < kStates.size(); ++state) {
    const auto packed = Pack(kStates[state]);
    std::println("field_state state={} cover={} east_m={} north_m={}",
                 state,
                 kStates[state].Cover,
                 kStates[state].EastM,
                 kStates[state].NorthM);
    const auto words = std::bit_cast<std::array<uint32_t, 4>>(packed);
    std::println("gpu_field_state state={} words={} {} {} {}",
                 state,
                 words[0],
                 words[1],
                 words[2],
                 words[3]);
    for (uint32_t repeat = 0; repeat <= repeats; ++repeat) {
      auto commands = Require(device.Begin());
      Require(commands->Dispatch(*data.Producer,
                                 Bindings(data, packed, true),
                                 {.X = (data.Coarse.Count + kGroupWidth - 1) / kGroupWidth}));
      Normalize(*commands, data, packed);
      Require(commands->Dispatch(*data.Reconstruction,
                                 Bindings(data, packed, false),
                                 {.X = (data.Image.Count + kGroupWidth - 1) / kGroupWidth}));
      if (repeat == repeats) { Copy(*commands, data); }
      const auto completion = Require(device.Submit(std::move(commands)));
      Complete(device, completion);
      Timings(device, completion, state, repeat, bool(data.Normalization));
    }
    const std::string prefix = std::string(output) + "-state-" + std::to_string(state);
    Save(device, *data.Readback, prefix + ".bin");
    Save(device, *data.FieldReadback, prefix + ".field");
  }
}
}

int main(int argc, char **argv) {
  if (argc != 7 && (argc != 9 || std::strcmp(argv[7], "--staged") != 0)) {
    std::println(
        stderr,
        "usage: cloud-air-transfer-gpu producer.spv reconstruct.spv "
        "image-fixtures field-fixtures output-prefix warm-repeats [--staged normalize.spv]");
    return 2;
  }
  uint32_t repeats = 0;
  const char *last = argv[6] + std::strlen(argv[6]);
  const auto parsed = std::from_chars(argv[6], last, repeats);
  if (parsed.ec != std::errc{} || parsed.ptr != last || repeats < 1 || repeats > 16) { return 2; }
  auto device = Require(CreateDevice({.Validation = true, .GpuTiming = true}));
  const auto data = Prepare(*device, argv + 1, argc == 9 ? argv[8] : nullptr);
  std::println("device={} image_rays={} field_rays={} field_bytes={}",
               device->Limits().Name,
               data.Image.Count,
               data.Coarse.Count,
               data.FieldReadback->Description().Bytes);
  MemoryModel(data);
  Run(*device, data, argv[5], repeats);
  return Report();
}

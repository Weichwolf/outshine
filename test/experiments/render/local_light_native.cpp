#include "NativeGpuCheck.h"
#include "Check.h"
#include "SubjectLightingUniform.h"
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using outshine::Render::SubjectLightingUniform;
constexpr uint32_t kWidth = 1920, kHeight = 1080;
constexpr size_t kPixelBytes = 4 * sizeof(float);
constexpr uint64_t kImageBytes = uint64_t{kWidth} * kHeight * kPixelBytes;
constexpr std::array<float, 4> kDepths{25, 250, 500, 5000};
constexpr std::array<const char *, 3> kMethods{"brute", "tiles", "log"};
constexpr double kNanosecondsPerMillisecond = 1'000'000;
constexpr size_t kMaximumInputBytes = size_t{32} * 1024 * 1024;
constexpr uint32_t kCoordinateThreads = 64;

struct alignas(16) ClusterFrame {
  std::array<uint32_t, 4> Grid;
  std::array<float, 4> Depth, ViewZ, Viewport;
};

struct alignas(16) ProbeFrame {
  std::array<float, 4> Right, Up, Forward, Eye, Control;
};

static_assert(sizeof(ClusterFrame) == 64 && alignof(ClusterFrame) == 16);
static_assert(offsetof(ClusterFrame, Grid) == 0 && offsetof(ClusterFrame, Depth) == 16);
static_assert(offsetof(ClusterFrame, ViewZ) == 32 &&
              offsetof(ClusterFrame, Viewport) == 3 * sizeof(std::array<float, 4>));
static_assert(sizeof(ProbeFrame) == 5 * sizeof(std::array<float, 4>) && alignof(ProbeFrame) == 16);
static_assert(offsetof(ProbeFrame, Control) == 64);

std::vector<uint32_t> Read(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff bytes = file ? file.tellg() : std::streampos(-1);
  const bool valid =
      bytes > 0 && std::cmp_less_equal(bytes, kMaximumInputBytes) && bytes % sizeof(uint32_t) == 0;
  CHECK(valid, "the input has a bounded whole-word size");
  if (!valid) { Failed(); }
  std::vector<uint32_t> words(static_cast<size_t>(bytes) / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(words.data()), bytes);
  CHECK(file.good(), "the complete input is readable");
  if (!file.good()) { Failed(); }
  return words;
}

template <typename T> T Parameter(const std::string &path) {
  const auto words = Read(path);
  CHECK(words.size() * sizeof(uint32_t) == sizeof(T), "the parameter size matches its shader");
  if (words.size() * sizeof(uint32_t) != sizeof(T)) { Failed(); }
  T result{};
  std::memcpy(&result, words.data(), sizeof(T));
  return result;
}

Completion Finish(Device &device, std::unique_ptr<Commands> commands) {
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(30))) == CompletionStatus::Ready,
        "the offline submission completes within its explicit bound");
  return done;
}

struct Inputs {
  SubjectLightingUniform Lighting{};
  ProbeFrame Probe{};
  std::array<ClusterFrame, 3> Frames{};
  std::unique_ptr<Buffer> Lights, Tiles, Clusters;
  std::unique_ptr<Texture> Shadow;
  std::unique_ptr<Sampler> Sampling;

  Inputs(Device &device, const std::string &folder) {
    Lighting = Parameter<SubjectLightingUniform>(folder + "/lighting.bin");
    Probe = Parameter<ProbeFrame>(folder + "/probe.bin");
    for (size_t i = 0; i < kMethods.size(); ++i) {
      Frames[i] = Parameter<ClusterFrame>(folder + "/" + kMethods[i] + "-frame.bin");
      CHECK(Frames[i].Viewport[2] == static_cast<float>(kWidth) &&
                Frames[i].Viewport[3] == static_cast<float>(kHeight),
            "all methods retain the full 1920 by 1080 profile");
    }
    auto commands = Require(device.Begin());
    Lights = Upload(device, *commands, Read(folder + "/lights.bin"));
    Tiles = Upload(device, *commands, Read(folder + "/tiles-lists.bin"));
    Clusters = Upload(device, *commands, Read(folder + "/log-lists.bin"));
    Shadow =
        Require(device.CreateTexture({.Width = 1,
                                      .Height = 1,
                                      .Texels = Format::R32Float,
                                      .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
    Sampling = Require(device.CreateSampler({}));
    auto staging = Require(device.CreateBuffer(
        {.Bytes = sizeof(float), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
    const float depth = 1;
    const auto mapped = Require(device.MapWrite(*staging, {.Bytes = sizeof(float)}));
    std::memcpy(mapped.data(), &depth, sizeof(depth));
    Require(device.Unmap(*staging));
    Require(commands->Copy(
        *staging, *Shadow, {.RowBytes = sizeof(float), .Region = {.Width = 1, .Height = 1}}));
    const auto done = Finish(device, std::move(commands));
    const auto timing = Require(device.SubmissionGpuTime(done));
    std::println("{{\"upload_gpu_ms\":{}}}",
                 timing.ElapsedNanoseconds / kNanosecondsPerMillisecond);
  }

  template <typename PipelineType>
  [[nodiscard]] std::vector<Binding> Bindings(const PipelineType &pipeline, size_t method) const {
    const std::array available{
        Binding{.Set = 2,
                .Slot = 0,
                .Value =
                    SampledTexture{.Image = {.Image = Shadow.get()}, .Sampling = Sampling.get()}},
        Storage(2, 24, *Lights),
        Storage(2, 25, method == 2 ? *Clusters : *Tiles),
        Binding{.Set = 3, .Slot = 1, .Value = Uniform{std::as_bytes(std::span(&Lighting, 1))}},
        Binding{
            .Set = 3, .Slot = 4, .Value = Uniform{std::as_bytes(std::span(&Frames[method], 1))}},
        Binding{.Set = 3, .Slot = 5, .Value = Uniform{std::as_bytes(std::span(&Probe, 1))}}};
    std::vector<Binding> result;
    for (const auto &expected : pipeline.Layout().Bindings) {
      const auto &resource = [](const auto &value) -> const auto & {
        if constexpr (requires { value.Resource; }) {
          return value.Resource;
        } else {
          return value;
        }
      }(expected);
      for (const auto &binding : available) {
        if (binding.Set == resource.Set && binding.Slot == resource.Slot) {
          result.push_back(binding);
        }
      }
    }
    CHECK(result.size() == pipeline.Layout().Bindings.size(),
          "every reflected binding has its actual resource");
    return result;
  }
};

struct DrawSample {
  size_t Method = 0, Plane = 0;
};

struct CoordinateUpdate {
  size_t Map = 0;
  int Origin = 0;
};

struct CoordinatePass {
  std::unique_ptr<ComputePipeline> State;
  std::array<int, 2> Origins{-1, -1};
  bool Motion = false, Stale = false;
  uint32_t Dispatches = 0;

  [[nodiscard]] std::optional<CoordinateUpdate>
  Record(Commands &commands, const Inputs &inputs, DrawSample sample) const {
    if (!State) { return std::nullopt; }
    const size_t map = sample.Method == 2 ? 1 : 0;
    const int origin =
        Motion && sample.Plane > 0 && sample.Plane < 3 ? static_cast<int>(sample.Plane) : 0;
    const int key = Stale ? 0 : origin;
    if (Origins[map] == key) { return std::nullopt; }
    const auto bindings = inputs.Bindings(*State, sample.Method);
    const auto count = static_cast<uint32_t>(inputs.Lighting.Count[0]);
    Require(commands.Dispatch(
        *State, bindings, {.X = (count + kCoordinateThreads - 1) / kCoordinateThreads}));
    return CoordinateUpdate{.Map = map, .Origin = key};
  }

  void Accept(const std::optional<CoordinateUpdate> &update) {
    if (!update) { return; }
    Origins[update->Map] = update->Origin;
    ++Dispatches;
  }
};

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const char *vertex, const char *fragment) {
  const auto vert = Spirv(vertex);
  const auto frag = Spirv(fragment);
  constexpr std::array colours{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  return Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vert},
                                                .Fragment = ShaderCode{.Spirv = frag},
                                                .Streams = {},
                                                .Attributes = {},
                                                .Colours = colours,
                                                .Depth = std::nullopt,
                                                .Culling = Cull::None,
                                                .Label = "LocalLightDirectControl"}));
}

void Capture(Device &device, Texture &image, const std::string &path) {
  auto readback = Require(device.CreateBuffer(
      {.Bytes = kImageBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      image,
      *readback,
      {.RowBytes = kWidth * kPixelBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
  Finish(device, std::move(commands));
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = kImageBytes}));
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(mapped.data()),
             static_cast<std::streamsize>(mapped.size()));
  CHECK(file.good(), "the actual full-resolution capture is written completely");
  Require(device.Unmap(*readback));
}

void Run(Device &device,
         const std::array<const GraphicsPipeline *, 3> &pipelines,
         Inputs &inputs,
         CoordinatePass &coordinates,
         const std::string &folder,
         uint32_t iterations) {
  auto image =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  const std::array targets{ColourAttachment{.Image = {.Image = image.get()}, .Begin = Load::Clear}};
  for (uint32_t iteration = 0; iteration < iterations; ++iteration) {
    for (size_t order = 0; order < pipelines.size(); ++order) {
      const size_t method = iteration % 2 == 0 ? order : pipelines.size() - 1 - order;
      for (size_t plane = 0; plane < kDepths.size(); ++plane) {
        inputs.Probe.Control[2] = kDepths[plane];
        const auto bindings = inputs.Bindings(*pipelines[method], method);
        auto commands = Require(device.Begin());
        const auto update =
            coordinates.Record(*commands, inputs, {.Method = method, .Plane = plane});
        Require(commands->BeginRenderPass({.Colours = targets, .Depth = std::nullopt}));
        Require(commands->Draw({.State = {.Pipeline = pipelines[method],
                                          .Bindings = bindings,
                                          .Vertices = {},
                                          .Area = {.Width = kWidth, .Height = kHeight},
                                          .Clip = {.Width = kWidth, .Height = kHeight}},
                                .Range = {.Count = 3}}));
        Require(commands->EndRenderPass());
        const auto done = Finish(device, std::move(commands));
        coordinates.Accept(update);
        const auto timing = Require(device.SubmissionGpuTime(done));
        CHECK(std::isfinite(timing.ElapsedNanoseconds) && timing.ElapsedNanoseconds > 0,
              "the complete optional-coordinate-plus-render submission has a real GPU interval");
        std::println(
            R"({{"method":"{}","plane_m":{},"iteration":{},"gpu_ms":{},"coordinate_dispatch":{}}})",
            kMethods[method],
            kDepths[plane],
            iteration,
            timing.ElapsedNanoseconds / kNanosecondsPerMillisecond,
            update.has_value());
        if (iteration + 1 == iterations) {
          Capture(device,
                  *image,
                  folder + "/" + kMethods[method] + "-" + std::to_string(plane) + ".rgba32");
        }
      }
    }
  }
  std::println(R"({{"coordinate_dispatches":{},"camera_motion":{},"stale_control":{}}})",
               coordinates.Dispatches,
               coordinates.Motion,
               coordinates.Stale);
}
}

int main(int argc, char **argv) {
  if (argc != 8 && argc != 11) { return 2; }
  if (argc == 11 && ((std::string(argv[9]) != "0" && std::string(argv[9]) != "1") ||
                     (std::string(argv[10]) != "0" && std::string(argv[10]) != "1"))) {
    return 2;
  }
  uint32_t iterations = 0;
  const std::string repeat = argv[6];
  const auto parsed = std::from_chars(repeat.data(), repeat.data() + repeat.size(), iterations);
  if (parsed.ec != std::errc{} || parsed.ptr != repeat.data() + repeat.size() || iterations == 0 ||
      iterations > 16) {
    return 2;
  }
  auto device =
      Require(CreateDevice({.Validation = std::string(argv[7]) == "1", .GpuTiming = true}));
  CHECK(device->Limits().GpuTimestamps, "the device supplies actual GPU timestamps");
  std::println("device={} width={} height={} iterations={}",
               device->Limits().Name,
               kWidth,
               kHeight,
               iterations);
  auto brute = Pipeline(*device, argv[1], argv[2]);
  auto listed = Pipeline(*device, argv[1], argv[3]);
  Inputs inputs(*device, argv[4]);
  CoordinatePass coordinates;
  if (argc == 11) {
    coordinates.State = Require(device->CreateComputePipeline(Spirv(argv[8])));
    coordinates.Motion = std::string(argv[9]) == "1";
    coordinates.Stale = std::string(argv[10]) == "1";
    const std::array block{kCoordinateThreads, 1u, 1u};
    CHECK(coordinates.State->Layout().LocalSize == block,
          "the coordinate workgroup matches its host dispatch");
  }
  Run(*device, {brute.get(), listed.get(), listed.get()}, inputs, coordinates, argv[5], iterations);
  return Report();
}

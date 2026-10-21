#include "device/native/Device.h"
#include "GpuSupport.h"
#include "SpirvFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <print>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace N = outshine::Render::Native;
namespace C = outshine::Diagnostics::Calibration;

namespace {
constexpr uint32_t Side = 64;
constexpr uint64_t Bytes = uint64_t{Side} * Side * sizeof(float);
constexpr float MinimumW = 1.0e-6f;
constexpr size_t ProgressInterval = 50;
constexpr uint64_t PercentScale = 100;
constexpr uint64_t MinimumCoveragePercent = 90;

struct alignas(16) Triangle {
  std::array<std::array<float, 4>, 3> Points;
  std::array<uint32_t, 4> Options{};
};

static_assert(sizeof(Triangle) == 64 && alignof(Triangle) == 16);
static_assert(offsetof(Triangle, Options) == size_t{3} * 4 * sizeof(float));

struct Point {
  long double X, Y, Z;
};

struct Counts {
  uint64_t Accepted = 0;
  uint64_t FullyCovered = 0;
  uint64_t FalseCoverage = 0;
  uint64_t FalseDepth = 0;
  uint64_t InvalidOutput = 0;
};

struct Timing {
  double GpuNs = 0;
  uint64_t Samples = 0;
  uint64_t Missing = 0;
};

[[noreturn]] void Fail(const N::Error &error) {
  std::println(stderr, "{}", error.Message);
  std::_Exit(2);
}

template <class T> T Take(N::Result<T> result) {
  if (!result) { Fail(result.error()); }
  return std::move(*result);
}

void Take(N::Result<void> result) {
  if (!result) { Fail(result.error()); }
}

std::vector<uint32_t> Shader(const std::filesystem::path &path) {
  auto words = outshine::Render::ReadSpirv(path.string());
  if (!words) {
    std::println(stderr, "{}", words.error());
    std::_Exit(2);
  }
  return std::move(*words);
}

long double Edge(const Point &a, const Point &b, const Point &p) {
  return (b.X - a.X) * (p.Y - a.Y) - (b.Y - a.Y) * (p.X - a.X);
}

std::array<Point, 3> Project(const Triangle &triangle) {
  std::array<Point, 3> result;
  for (size_t i = 0; i < 3; ++i) {
    const auto &p = triangle.Points[i];
    result[i] = {.X = (static_cast<long double>(p[0]) / p[3] + 1) * Side / 2,
                 .Y = (1 - static_cast<long double>(p[1]) / p[3]) * Side / 2,
                 .Z = static_cast<long double>(p[2]) / p[3]};
  }
  return result;
}

bool Usable(const Triangle &triangle) {
  for (const auto &p : triangle.Points) {
    for (const float v : p) {
      if (!std::isfinite(v)) { return false; }
    }
    if (p[3] <= MinimumW || p[2] < 0 || p[2] > p[3]) { return false; }
  }
  return true;
}

std::optional<long double> PixelDepth(const std::array<Point, 3> &points, uint32_t x, uint32_t y) {
  const long double area = Edge(points[0], points[1], points[2]);
  if (area == 0) { return {}; }
  long double least = 1;
  for (size_t corner = 0; corner < 4; ++corner) {
    const Point p{.X = static_cast<long double>(x) + (corner & 1u),
                  .Y = static_cast<long double>(y) + ((corner >> 1u) & 1u),
                  .Z = 0};
    const auto a = Edge(points[1], points[2], p) / area;
    const auto b = Edge(points[2], points[0], p) / area;
    const auto c = Edge(points[0], points[1], p) / area;
    if (a < 0 || b < 0 || c < 0) { return {}; }
    least = std::min(least, a * points[0].Z + b * points[1].Z + c * points[2].Z);
  }
  return least;
}

void Check(const Triangle &triangle, std::span<const float> output, Counts &counts) {
  const bool usable = Usable(triangle);
  std::array<Point, 3> points{};
  if (usable) { points = Project(triangle); }
  for (uint32_t y = 0; y < Side; ++y) {
    for (uint32_t x = 0; x < Side; ++x) {
      const float value = output[size_t{y} * Side + x];
      counts.InvalidOutput += !std::isfinite(value) || value < 0 || value > 1 ? 1u : 0u;
      const bool accepted = value > 0;
      const auto least = usable ? PixelDepth(points, x, y) : std::nullopt;
      const bool covered = least.has_value();
      counts.FullyCovered += covered ? 1u : 0u;
      counts.Accepted += accepted ? 1u : 0u;
      counts.FalseCoverage += accepted && !covered ? 1u : 0u;
      counts.FalseDepth +=
          accepted && covered && static_cast<long double>(value) > *least ? 1u : 0u;
    }
  }
}

std::vector<float> Draw(N::Device &device,
                        N::GraphicsPipeline &pipeline,
                        N::Texture &image,
                        N::Buffer &readback,
                        const Triangle &triangle,
                        Timing &timing) {
  auto sample = Take(C::Measure(device, [&](N::Commands &commands) -> N::Result<void> {
    const std::array colours{N::ColourAttachment{
        .Image = {.Image = &image}, .Begin = N::Load::Clear, .End = N::Store::Store}};
    N::RenderPassDescription pass;
    pass.Colours = colours;
    auto begun = commands.BeginRenderPass(pass);
    if (!begun) { return begun; }
    const std::array bindings{N::Binding{
        .Set = 1, .Slot = 0, .Value = N::Uniform{.Bytes = std::as_bytes(std::span(&triangle, 1))}}};
    N::DrawDescription draw;
    draw.State.Pipeline = &pipeline;
    draw.State.Bindings = bindings;
    draw.State.Area.Width = Side;
    draw.State.Area.Height = Side;
    draw.State.Clip.Width = Side;
    draw.State.Clip.Height = Side;
    draw.Range.Count = 3;
    auto drawn = commands.Draw(draw);
    if (!drawn) { return drawn; }
    return commands.EndRenderPass();
  }));
  if (sample.GpuNs) {
    timing.GpuNs += *sample.GpuNs;
    ++timing.Samples;
  } else {
    ++timing.Missing;
  }
  auto commands = Take(device.Begin());
  N::TextureTransfer transfer;
  transfer.RowBytes = uint64_t{Side} * sizeof(float);
  transfer.Region.Width = Side;
  transfer.Region.Height = Side;
  Take(commands->Copy(image, readback, transfer));
  Take(C::Finish(device, std::move(commands)));
  auto mapped = Take(device.MapRead(readback, {.Bytes = Bytes}));
  std::vector<float> output(size_t{Side} * Side);
  std::memcpy(output.data(), mapped.data(), Bytes);
  Take(device.Unmap(readback));
  return output;
}

Triangle FromPixels(std::array<Point, 3> points, std::array<float, 3> weights) {
  Triangle triangle;
  for (size_t i = 0; i < 3; ++i) {
    triangle.Points[i] = {static_cast<float>((2 * points[i].X / Side - 1) * weights[i]),
                          static_cast<float>((1 - 2 * points[i].Y / Side) * weights[i]),
                          static_cast<float>(points[i].Z * weights[i]),
                          weights[i]};
  }
  return triangle;
}

void ShiftedCases(std::vector<Triangle> &result) {
  constexpr std::array<Point, 3> regular{
      {{.X = 4, .Y = 4, .Z = .9}, {.X = 60, .Y = 8, .Z = .4}, {.X = 16, .Y = 60, .Z = .2}}};
  constexpr std::array<Point, 3> outside{
      {{.X = -20, .Y = -20, .Z = .8}, {.X = 84, .Y = -20, .Z = .2}, {.X = -20, .Y = 84, .Z = .4}}};
  constexpr std::array<Point, 3> thin{
      {{.X = 4, .Y = 30, .Z = .8}, {.X = 60, .Y = 30.2L, .Z = .7}, {.X = 32, .Y = 30.4L, .Z = .2}}};
  for (int offset = 0; offset < 16; ++offset) {
    const long double shift = static_cast<long double>(offset) / 16;
    auto points = regular;
    points[0].X += shift;
    points[1].Y += shift;
    result.push_back(FromPixels(points, {1, 2, 4}));
    points = outside;
    points[0].X += shift;
    result.push_back(FromPixels(points, {.5, 4, 2}));
    points = thin;
    for (auto &point : points) { point.Y += shift; }
    result.push_back(FromPixels(points, {1, 1, 1}));
  }
}

void RandomCases(std::vector<Triangle> &result, uint32_t seed) {
  constexpr uint32_t positionSteps = 1025;
  constexpr long double positionSubdivisions = 8;
  constexpr long double positionOrigin = -32;
  constexpr long double minimumDepth = .05L;
  constexpr uint32_t depthSteps = 850;
  constexpr long double depthSubdivisions = 1000;
  constexpr uint32_t weightPowers = 5;
  constexpr uint32_t weightFractionSteps = 997;
  constexpr float weightFractionDenominator = weightFractionSteps;
  std::mt19937 random(seed);
  for (size_t i = 0; i < 256; ++i) {
    std::array<Point, 3> points;
    std::array<float, 3> weights;
    for (size_t v = 0; v < 3; ++v) {
      points[v] = {.X = static_cast<long double>(random() % positionSteps) / positionSubdivisions +
                        positionOrigin,
                   .Y = static_cast<long double>(random() % positionSteps) / positionSubdivisions +
                        positionOrigin,
                   .Z = minimumDepth +
                        static_cast<long double>(random() % depthSteps) / depthSubdivisions};
      weights[v] = std::ldexp(1.0f, static_cast<int>(random() % weightPowers) - 1);
      weights[v] *=
          1 + static_cast<float>(random() % weightFractionSteps) / weightFractionDenominator;
    }
    result.push_back(FromPixels(points, weights));
  }
}

void UnknownCases(std::vector<Triangle> &result) {
  constexpr float outsideNear = -.1f;
  auto clipped = result.front();
  clipped.Points[0][2] = outsideNear;
  result.push_back(clipped);
  clipped = result.front();
  clipped.Points[0][3] = 0;
  result.push_back(clipped);
  clipped = result.front();
  clipped.Points[0][2] = 2 * clipped.Points[0][3];
  result.push_back(clipped);
  clipped = result.front();
  clipped.Points[0][3] = -1;
  result.push_back(clipped);
  for (size_t vertex = 0; vertex < 3; ++vertex) {
    for (size_t component = 0; component < 4; ++component) {
      for (const float invalid : {std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::quiet_NaN()}) {
        clipped = result.front();
        clipped.Points[vertex][component] = invalid;
        result.push_back(clipped);
      }
    }
  }
}

std::vector<Triangle> Cases(uint32_t seed) {
  std::vector<Triangle> result;
  ShiftedCases(result);
  RandomCases(result, seed);
  UnknownCases(result);
  constexpr std::array aligned{
      std::array<Point, 3>{
          {{.X = 8, .Y = 8, .Z = .5}, {.X = 56, .Y = 8, .Z = .5}, {.X = 8, .Y = 56, .Z = .5}}},
      std::array<Point, 3>{
          {{.X = 8, .Y = 8, .Z = .5}, {.X = 8, .Y = 56, .Z = .5}, {.X = 56, .Y = 8, .Z = .5}}},
      std::array<Point, 3>{
          {{.X = 8, .Y = 8, .Z = .5}, {.X = 8, .Y = 8, .Z = .5}, {.X = 56, .Y = 8, .Z = .5}}},
      std::array<Point, 3>{
          {{.X = 8, .Y = 8, .Z = .5}, {.X = 32, .Y = 8, .Z = .5}, {.X = 56, .Y = 8, .Z = .5}}}};
  for (const auto &points : aligned) { result.push_back(FromPixels(points, {1, 1, 1})); }
  return result;
}
}

int main(int argc, char **argv) {
  if (argc != 4) { return 2; }
  const std::string_view seedText(argv[3]);
  uint32_t seed = 0;
  const auto parsed = std::from_chars(seedText.data(), seedText.data() + seedText.size(), seed);
  if (parsed.ec != std::errc{} || parsed.ptr != seedText.data() + seedText.size()) { return 2; }
  const std::filesystem::path folder(argv[1]);
  auto vertex = Shader(folder / "coverage.vert.spv");
  auto fragment = Shader(folder / "coverage.frag.spv");
  auto device = Take(N::CreateDevice({.Validation = true, .GpuTiming = true}));
  auto image = Take(
      device->CreateTexture({.Width = Side,
                             .Height = Side,
                             .Texels = N::Format::R32Float,
                             .Use = N::TextureUse::ColourAttachment | N::TextureUse::CopySource}));
  auto readback = Take(device->CreateBuffer(
      {.Bytes = Bytes, .Use = N::BufferUse::CopyDestination, .Placement = N::Memory::Readback}));
  N::ColourTarget target;
  target.Texels = N::Format::R32Float;
  const std::array targets{target};
  N::GraphicsDescription desc;
  desc.Vertex.Spirv = vertex;
  desc.Fragment = N::ShaderCode{.Spirv = fragment};
  desc.Colours = targets;
  desc.Culling = N::Cull::None;
  auto pipeline = Take(device->CreateGraphicsPipeline(desc));
  std::array<Counts, 2> counts{};
  std::array<Timing, 2> timing{};
  auto cases = Cases(seed);
  const auto began = std::chrono::steady_clock::now();
  for (size_t i = 0; i < cases.size(); ++i) {
    for (uint32_t mode = 0; mode < 2; ++mode) {
      cases[i].Options[0] = mode;
      auto values = Draw(*device, *pipeline, *image, *readback, cases[i], timing[mode]);
      Check(cases[i], values, counts[mode]);
    }
    if (i % ProgressInterval == 0) {
      std::println("case {}/{}", i, cases.size());
      std::fflush(stdout);
    }
  }
  nlohmann::json rows = nlohmann::json::array();
  for (size_t i = 0; i < 2; ++i) {
    const auto &c = counts[i];
    rows.push_back(nlohmann::json{{"mode", i},
                                  {"accepted", c.Accepted},
                                  {"fully_covered", c.FullyCovered},
                                  {"false_coverage", c.FalseCoverage},
                                  {"false_depth", c.FalseDepth},
                                  {"invalid_output", c.InvalidOutput},
                                  {"gpu_ns", timing[i].GpuNs},
                                  {"gpu_samples", timing[i].Samples},
                                  {"missing_gpu_samples", timing[i].Missing}});
  }
  std::ofstream output(argv[2]);
  output
      << nlohmann::
             json{{"device", device->Limits().Name},
                  {"cases", cases.size()},
                  {"seed", seed},
                  {"side_pixels", Side},
                  {"readback_bytes", Bytes * cases.size() * 2},
                  {"wall_seconds",
                   std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count()},
                  {"modes", rows},
                  {"scope",
                   "Projected finite homogeneous triangles only; no current world/Metal/general "
                   "floating-error proof. Near/far/eye crossings retain unknown coverage."}}
                 .dump(2);
  std::println("point false {} conservative false {} depth {}",
               counts[0].FalseCoverage,
               counts[1].FalseCoverage,
               counts[1].FalseDepth);
  return output && counts[0].FalseCoverage > 0 && counts[1].FullyCovered > 0 &&
                 counts[1].Accepted * PercentScale >=
                     counts[1].FullyCovered * MinimumCoveragePercent &&
                 counts[1].FalseCoverage == 0 && counts[1].FalseDepth == 0 &&
                 counts[0].InvalidOutput == 0 && counts[1].InvalidOutput == 0
             ? 0
             : 1;
}

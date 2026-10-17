#include "local_light_build.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <ios>
#include <iosfwd>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <utility>

namespace {
using namespace LightBuild;
constexpr size_t kMaximumBytes = size_t{32} * 1024 * 1024;
constexpr double kMillisecondsPerSecond = 1000;
constexpr uint32_t kWidth = 1920, kHeight = 1080, kSlices = 24;
constexpr size_t kMaximumRepetitions = 1000;
constexpr double kMaximumCoordinate = 1e8;
constexpr double kMinimumPositive = 1 / kMaximumCoordinate;

template <typename T> std::optional<std::vector<T>> Read(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff bytes = file ? file.tellg() : std::streampos(-1);
  if (bytes <= 0 || std::cmp_greater(bytes, kMaximumBytes) || bytes % sizeof(T) != 0) {
    return std::nullopt;
  }
  std::vector<T> data(static_cast<size_t>(bytes) / sizeof(T));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(data.data()), bytes);
  if (!file) { return std::nullopt; }
  return data;
}

template <typename T> bool Write(const std::string &path, std::span<const T> data) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size_bytes()));
  file.close();
  return static_cast<bool>(file);
}

std::optional<size_t> Number(std::string_view text) {
  size_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) { return std::nullopt; }
  return value;
}

bool Valid(std::span<const Light> lights, const Camera &camera, std::span<const double> edges) {
  const auto bounded = [](double value) {
    return std::isfinite(value) && std::abs(value) <= kMaximumCoordinate;
  };
  if (!std::ranges::all_of(camera.Values, bounded) ||
      !std::ranges::all_of(edges.first(edges.size() - 1), bounded) || !std::isinf(edges.back()) ||
      edges.back() < 0 || std::ranges::adjacent_find(edges, std::greater_equal{}) != edges.end()) {
    return false;
  }
  if (edges.front() < kMinimumPositive || camera.Values[12] < kMinimumPositive ||
      camera.Values[13] < kMinimumPositive || camera.Grid[0] != kWidth ||
      camera.Grid[1] != kHeight || camera.Grid[2] > 1 || camera.Grid[3] != 64 ||
      edges.size() > kSlices + 1) {
    return false;
  }
  return std::ranges::all_of(lights, [bounded](const auto &light) {
    return std::ranges::all_of(light, bounded) &&
           (light[3] == 0 || light[3] == 1 || light[3] == 2) &&
           (light[7] == 0 || light[7] >= kMinimumPositive);
  });
}

double Milliseconds(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() *
         kMillisecondsPerSecond;
}

void Measure(Builder &builder,
             std::span<const Light> lights,
             const Camera &camera,
             std::span<const double> edges,
             size_t budget,
             Method method,
             size_t iteration) {
  const auto start = std::chrono::steady_clock::now();
  builder.Build(lights, camera, edges, budget, method);
  std::atomic_signal_fence(std::memory_order_seq_cst);
  const double elapsed = Milliseconds(start);
  std::println(
      R"({{"iteration":{},"work_counters":{},"build_ms":{},"cell_tests":{},"block_tests":{},"assignments_before_fallback":{},"overflow":{},"output_bytes":{},"own_allocations":{},"own_allocated_bytes":{},"own_live_bytes":{},"own_peak_bytes":{},"growth_retained_bytes":{}}})",
      iteration,
      kWorkCounters,
      elapsed,
      builder.Tested,
      builder.BlockTests,
      builder.Assignments,
      builder.Overflow,
      builder.Words.size() * sizeof(uint32_t),
      builder.Heap.Allocations,
      builder.Heap.AllocatedBytes,
      builder.Heap.LiveBytes,
      builder.Heap.PeakBytes,
      builder.GrowthRetainedBytes);
}

void Repack(std::span<const Light> lights,
            std::span<Light> packed,
            const Camera &camera,
            size_t iteration) {
  const auto start = std::chrono::steady_clock::now();
  std::memcpy(packed.data(), lights.data(), lights.size_bytes());
  for (size_t light = 0; light < lights.size(); ++light) {
    for (size_t axis = 0; axis < 3; ++axis) {
      packed[light][4 + axis] = static_cast<float>(static_cast<double>(lights[light][4 + axis]) -
                                                   camera.Values[9 + axis]);
    }
  }
  std::atomic_signal_fence(std::memory_order_seq_cst);
  const double elapsed = Milliseconds(start);
  std::println(
      R"({{"iteration":{},"repack_ms":{},"source_memcpy_bytes":{},"position_components":{}}})",
      iteration,
      elapsed,
      lights.size_bytes(),
      lights.size() * 3);
}
}

int main(int argc, char **argv) {
  if (argc != 7) {
    std::println(stderr,
                 "input-folder output-prefix sort|scatter|hierarchy tiles|log index-bytes repeats");
    return 2;
  }
  const std::string input = argv[1];
  const std::string output = argv[2];
  const std::string_view method = argv[3];
  const std::string_view grid = argv[4];
  const auto budget = Number(argv[5]);
  const auto repetitions = Number(argv[6]);
  if (!budget || !repetitions || *budget == 0 || *budget > kMaximumBytes || *repetitions == 0 ||
      *repetitions > kMaximumRepetitions ||
      (method != "sort" && method != "scatter" && method != "hierarchy") ||
      (grid != "tiles" && grid != "log")) {
    return 2;
  }
  const auto lights = Read<Light>(input + "/lights.bin");
  const auto values = Read<double>(input + "/builder-camera.bin");
  const auto dimensions = Read<uint32_t>(input + "/builder-grid.bin");
  const auto edges = Read<double>(input + "/" + std::string(grid) + "-builder-edges.bin");
  if (!lights || !values || !dimensions || !edges || values->size() != 16 ||
      dimensions->size() != 4 || edges->size() < 2) {
    return 2;
  }
  Camera camera{};
  std::copy(values->begin(), values->end(), camera.Values.begin());
  std::copy(dimensions->begin(), dimensions->end(), camera.Grid.begin());
  camera.Perspective = camera.Grid[2] != 0;
  if (!Valid(*lights, camera, *edges)) { return 2; }
  Builder builder;
  std::vector<Light> packed(lights->size());
  for (size_t iteration = 0; iteration < *repetitions; ++iteration) {
    Method strategy = Method::Hierarchy;
    if (method == "sort") { strategy = Method::Sort; }
    if (method == "scatter") { strategy = Method::Scatter; }
    Measure(builder, *lights, camera, *edges, *budget, strategy, iteration);
    Repack(*lights, packed, camera, iteration);
  }
  if (!Write(output + ".words.bin", std::span<const uint32_t>(builder.Words)) ||
      !Write(output + ".repacked.bin", std::span<const Light>(packed))) {
    return 2;
  }
  return 0;
}

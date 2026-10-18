#include "local_light_world_build.h"
#include "local_light_build.h"
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

template <typename T> std::expected<T, int> Number(std::string_view value) {
  T result{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
    return std::unexpected(2);
  }
  return result;
}

template <typename T> std::expected<std::vector<T>, int> Read(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff size = file ? file.tellg() : std::streampos(-1);
  constexpr std::streamoff limit = std::streamoff{32} * 1024 * 1024;
  if (size < 0 || size > limit || size % sizeof(T) != 0) { return std::unexpected(2); }
  std::vector<T> result(static_cast<size_t>(size) / sizeof(T));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(result.data()), size);
  if (!file.good()) { return std::unexpected(2); }
  return result;
}

template <typename T> bool Write(const std::string &path, const std::pmr::vector<T> &value) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(value.data()),
             static_cast<std::streamsize>(value.size() * sizeof(T)));
  return file.good();
}

bool Dump(const LightWorld::Builder &builder, const std::string &prefix) {
  return Write(prefix + ".words.bin", builder.Words) &&
         Write(prefix + ".lights.bin", builder.Local) &&
         Write(prefix + ".cells.bin", builder.Coordinates);
}

double Milliseconds(Clock::time_point first, Clock::time_point last) {
  return std::chrono::duration<double, std::milli>(last - first).count();
}

void Report(const LightWorld::Builder &builder,
            std::string_view phase,
            size_t step,
            double buildMs,
            double packMs) {
  std::println(
      R"({{"phase":"{}","step":{},"build_ms":{},"pack_ms":{},"work_counters":{},"sphere_box_tests":{},"membership_changes":{},"assignments":{},"occupied_cells":{},"word_bytes":{},"own_allocations":{},"own_allocated_bytes":{},"own_live_bytes":{},"own_peak_bytes":{},"overflow":{}}})",
      phase,
      step,
      buildMs,
      packMs,
      LightBuild::kWorkCounters,
      builder.Tests,
      builder.MembershipChanges,
      builder.Assignments,
      builder.Cells.size(),
      builder.Words.size() * sizeof(uint32_t),
      builder.Heap.Allocations,
      builder.Heap.AllocatedBytes,
      builder.Heap.LiveBytes,
      builder.Heap.PeakBytes,
      builder.Overflow);
}

struct Options {
  uint32_t Width = 0, Repeats = 0;
  size_t Budget = 0;
  LightWorld::Vector Translation{};
  LightWorld::Lookup Method = LightWorld::Lookup::Hash;
};

std::expected<Options, int> Parse(char **argv) {
  const auto width = Number<uint32_t>(argv[3]);
  const auto budget = Number<size_t>(argv[5]);
  const auto repeats = Number<uint32_t>(argv[9]);
  if (!width || !budget || !repeats || *repeats == 0 || *repeats > 16) {
    return std::unexpected(2);
  }
  Options result{.Width = *width, .Repeats = *repeats, .Budget = *budget};
  const std::string_view method = argv[4];
  if (method != "hash" && method != "sorted") { return std::unexpected(2); }
  result.Method = method == "hash" ? LightWorld::Lookup::Hash : LightWorld::Lookup::Sorted;
  for (size_t axis = 0; axis < 3; ++axis) {
    const auto value = Number<double>(argv[6 + axis]);
    if (!value) { return std::unexpected(2); }
    result.Translation[axis] = *value;
  }
  return result;
}

bool Changes(LightWorld::Builder &builder,
             const std::vector<uint32_t> &words,
             const Options &options,
             const std::string &prefix) {
  constexpr size_t stride = 17;
  if (words.size() % stride != 0) { return false; }
  for (size_t step = 0; step < words.size() / stride; ++step) {
    const uint32_t id = words[step * stride];
    LightWorld::Light light{};
    for (size_t component = 0; component < light.size(); ++component) {
      light[component] = std::bit_cast<float>(words[step * stride + 1 + component]);
    }
    builder.Heap.Reset();
    const auto start = Clock::now();
    if (!builder.Update(id, light)) { return false; }
    const auto update = Clock::now();
    if (!builder.Pack(options.Method)) { return false; }
    const auto packed = Clock::now();
    Report(builder, "update", step, Milliseconds(start, update), Milliseconds(update, packed));
    if (!Dump(builder, prefix + ".update-" + std::to_string(step))) { return false; }
  }
  return true;
}
}

int main(int argc, char **argv) {
  if (argc != 10 && argc != 11) { return 2; }
  const auto options = Parse(argv);
  const auto source = Read<LightWorld::Light>(argv[1]);
  if (!options || !source) { return 2; }
  LightWorld::Builder builder;
  for (uint32_t iteration = 0; iteration < options->Repeats; ++iteration) {
    builder.Heap.Reset();
    const auto start = Clock::now();
    if (!builder.Build(*source, options->Width, options->Budget, options->Translation)) {
      return 2;
    }
    const auto built = Clock::now();
    if (!builder.Pack(options->Method)) { return 2; }
    const auto packed = Clock::now();
    Report(builder, "build", iteration, Milliseconds(start, built), Milliseconds(built, packed));
  }
  if (!Dump(builder, argv[2])) { return 2; }
  if (argc == 11) {
    const auto updates = Read<uint32_t>(argv[10]);
    if (!updates || !Changes(builder, *updates, *options, argv[2])) { return 2; }
  }
  return 0;
}

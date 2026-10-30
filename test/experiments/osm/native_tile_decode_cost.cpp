#include "CppAllocationProbe.h"
#include "OsmField.h"
#include "ReadTextFile.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <print>
#include <ratio>
#include <span>
#include <string_view>

namespace {
int Measure(std::string_view path) {
  constexpr size_t bytesMost = size_t{128} * 1024 * 1024;
  auto input = outshine::ReadTextFile(path, bytesMost);
  if (!input) {
    std::println(stderr, "{}", input.error());
    return 2;
  }
  const auto bytes = std::span(reinterpret_cast<const uint8_t *>(input->data()), input->size());
  std::array<double, 7> elapsed{};
  outshine::Test::CppAllocationCounts allocated;
  size_t heap = 0;
  size_t features = 0;
  size_t points = 0;
  for (double &milliseconds : elapsed) {
    const auto started = std::chrono::steady_clock::now();
    outshine::Test::BeginCppAllocationProbe();
    auto field = outshine::Generators::Osm::OsmField::DecodeNativeTile(bytes, bytesMost);
    const auto cost = outshine::Test::EndCppAllocationProbe();
    milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    if (!field) {
      std::println(stderr, "native tile decode rejected: {}", path);
      return 3;
    }
    allocated = cost;
    heap = field->HeapBytes();
    features = field->Features().size();
    points = field->Points().size() / 2;
  }
  std::ranges::sort(elapsed);
  std::println("{{\"native_bytes\":{},\"model_capacity_bytes\":{},\"cpp_allocation_calls\":{},"
               "\"cpp_allocated_total_bytes\":{},\"features\":{},\"points\":{},"
               "\"samples\":7,\"median_decode_ms\":{:.9g}}}",
               bytes.size(),
               heap,
               allocated.Calls,
               allocated.Bytes,
               features,
               points,
               elapsed[3]);
  return 0;
}
}

int main(int argc, const char *const *argv) {
  if (argc != 2) {
    std::println(stderr, "usage: native_tile_decode_cost native-tile.bin");
    return 1;
  }
  return Measure(argv[1]);
}

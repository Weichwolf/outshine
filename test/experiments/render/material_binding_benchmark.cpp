#include "MaterialResolution.h"
#include "CppAllocationProbe.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>
#include <vector>

namespace {
void Measure(size_t count, std::string_view mode) {
  using namespace outshine;
  std::vector<Material> surfaces(count);
  std::vector<Render::ShapePart> parts(mode == "unused" ? 0 : count);
  for (size_t at = 0; at < parts.size(); ++at) {
    parts[at].Material = static_cast<int>(mode == "repeated" ? at % 8 : (at * 17) % count);
  }
  Render::SurfaceTable table;
  Render::Shape shape;
  shape.Parts = parts;
  shape.Surfaces = surfaces;
  const auto began = std::chrono::steady_clock::now();
  Test::BeginCppAllocationProbe();
  Render::ResolveDeclaredSurface(shape, {}, table);
  const auto allocations = Test::EndCppAllocationProbe();
  const auto elapsed = std::chrono::steady_clock::now() - began;
  uint64_t digest = 14695981039346656037ULL;
  for (const int value : table.Material) {
    digest = (digest ^ static_cast<uint32_t>(value)) * 1099511628211ULL;
  }
  for (const uint32_t value : table.PartSlot) { digest = (digest ^ value) * 1099511628211ULL; }
  std::println("{{\"mode\":\"{}\",\"count\":{},\"slots\":{},\"digest\":{},"
               "\"allocation_calls\":{},\"requested_bytes\":{},\"wall_ns\":{}}}",
               mode,
               count,
               table.Slots.size(),
               digest,
               allocations.Calls,
               allocations.Bytes,
               std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}
}

int main() {
  for (const size_t count : {256, 1024, 4096, 16384}) {
    for (const std::string_view mode : {"dense", "repeated", "unused"}) { Measure(count, mode); }
  }
}

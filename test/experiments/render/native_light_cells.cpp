#include "LightCells.h"
#include "SubjectTypes.h"
#include "scene/PunctualLight.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <expected>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <system_error>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
constexpr size_t kBudget = size_t{16} * 1024 * 1024;

bool Dump(const char *path, std::span<const std::byte> bytes) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return file.good();
}

std::expected<std::vector<SubjectLight>, int> Read(const char *path, char **translation) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff bytes = file ? file.tellg() : std::streampos(-1);
  if (bytes < 0 || std::cmp_greater(bytes, kBudget) || bytes % 64 != 0) {
    return std::unexpected(2);
  }
  Vec3 shift;
  for (size_t axis = 0; axis < 3; ++axis) {
    const std::string value = translation[axis];
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), shift[axis]);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
      return std::unexpected(2);
    }
  }
  std::vector<SubjectLight> result(static_cast<size_t>(bytes) / 64);
  file.seekg(0);
  for (auto &source : result) {
    std::array<float, 16> words{};
    file.read(reinterpret_cast<char *>(words.data()), sizeof(words));
    if (!file.good()) { return std::unexpected(2); }
    if (!std::ranges::all_of(words, [](float value) { return std::isfinite(value); }) ||
        (words[3] != 0 && words[3] != 1 && words[3] != 2) || words[7] < 0) {
      return std::unexpected(2);
    }
    auto &light = source.Light;
    light.Kind = static_cast<LightKind>(static_cast<int>(words[3]));
    light.RangeM = words[7] > 0 ? 1.0f / words[7] : 0;
    light.Intensity = std::max({words[0], words[1], words[2]});
    for (size_t axis = 0; axis < 3; ++axis) {
      light.Colour[axis] = light.Intensity > 0 ? words[axis] / light.Intensity : 0;
      light.Direction[axis] = words[8 + axis];
      source.PositionEcefM[axis] = words[4 + axis] + shift[axis];
    }
    light.OuterConeRad = std::acos(std::clamp(words[12], -1.0f, 1.0f));
    const float innerCos = words[13] > 0 ? words[12] + 1.0f / words[13] : words[12];
    light.InnerConeRad = std::acos(std::clamp(innerCos, -1.0f, 1.0f));
  }
  return result;
}
}

int main(int argc, char **argv) {
  if (argc != 8) { return 2; }
  const auto source = Read(argv[1], argv + 5);
  if (!source) { return source.error(); }
  const LightCellLimits limits{.EmitterBytes = kBudget,
                               .IndexBytes = kBudget,
                               .ScratchBytes = kBudget,
                               .SphereBoxTests = 1'000'000};
  const auto start = std::chrono::steady_clock::now();
  const auto data = BuildLightCells(*source, limits);
  const auto end = std::chrono::steady_clock::now();
  if (!data) {
    std::println(stderr, "{}", data.error());
    return 1;
  }
  if (!Dump(argv[2], std::as_bytes(std::span(data->Words))) ||
      !Dump(argv[3], std::as_bytes(std::span(data->Emitters)))) {
    return 2;
  }
  std::ofstream positions(argv[4], std::ios::binary);
  for (const auto &light : *source) {
    const auto row = light.PositionEcefM.Row();
    positions.write(reinterpret_cast<const char *>(row.data()),
                    static_cast<std::streamsize>(row.size_bytes()));
  }
  std::println(
      R"({{"build_ms":{},"sphere_box_tests":{},"assignments":{},"packed_bytes":{},"scratch_bytes":{},"allocations":{},"scratch_copy_bytes":{},"full":{}}})",
      std::chrono::duration<double, std::milli>(end - start).count(),
      data->Costs.SphereBoxTests,
      data->Costs.Assignments,
      data->Costs.PackedBytes,
      data->Costs.ScratchBytes,
      data->Costs.Allocations,
      data->Costs.ScratchCopyBytes,
      data->FullEnumeration);
  return positions.good() ? 0 : 2;
}

#ifndef OUTSHINE_EXPERIMENTS_PROJECTION_SUPPORT_H
#define OUTSHINE_EXPERIMENTS_PROJECTION_SUPPORT_H
#include "device/native/Device.h"
#include "SpirvFile.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <print>
#include <string_view>
#include <utility>
#include <vector>

namespace Projection {
namespace N = outshine::Render::Native;
using Json = nlohmann::json;
using Matrix = std::array<float, 16>;
using Vector = std::array<float, 4>;

struct alignas(16) Input {
  Matrix Model{};
  Matrix View{};
  Vector Shift{};
  Vector Point{};
  std::array<uint32_t, 4> Options{};
};

static_assert(sizeof(Input) == sizeof(Matrix) * 2 + sizeof(Vector) * 3 && alignof(Input) == 16);
static_assert(offsetof(Input, View) == 64 && offsetof(Input, Shift) == 128);
static_assert(offsetof(Input, Point) == sizeof(Matrix) * 2 + sizeof(Vector) &&
              offsetof(Input, Options) == sizeof(Matrix) * 2 + sizeof(Vector) * 2);

[[noreturn]] inline void Fail(const N::Error &error) {
  std::println(stderr, "{}", error.Message);
  std::_Exit(2);
}

template <class T> T Take(N::Result<T> result) {
  if (!result) { Fail(result.error()); }
  return std::move(*result);
}

inline void Take(N::Result<void> result) {
  if (!result) { Fail(result.error()); }
}

inline std::vector<uint32_t> Shader(const std::filesystem::path &file) {
  auto words = outshine::Render::ReadSpirv(file.string());
  if (!words) {
    std::println(stderr, "{}", words.error());
    std::_Exit(2);
  }
  return std::move(*words);
}

[[noreturn]] inline void InvalidInput(std::string_view why) {
  std::println(stderr, "{}", why);
  std::_Exit(2);
}

template <size_t Size> std::array<float, Size> Floats(const Json &value) {
  if (!value.is_array() || value.size() != Size) { InvalidInput("invalid float array size"); }
  std::array<float, Size> result{};
  for (size_t i = 0; i < Size; ++i) {
    if (!value[i].is_number()) { InvalidInput("non-numeric coordinate"); }
    const auto number = value[i].get<double>();
    if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max()) {
      InvalidInput("coordinate outside finite float32");
    }
    result[i] = static_cast<float>(number);
  }
  return result;
}

}
#endif

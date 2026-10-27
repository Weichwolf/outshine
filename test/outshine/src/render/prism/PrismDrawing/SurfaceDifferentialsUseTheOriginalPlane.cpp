#include "Check.h"
#include "CapturedPrismPlanes.h"
#include "math/Mat4.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

using outshine::Test::PrismDifferential::Input;
using outshine::Test::PrismDifferential::kCapturedPlanes;

using Complex = std::complex<double>;
using Vector = std::array<Complex, 4>;
constexpr double kComplexStep = 1e-20;
constexpr uint32_t kPoison = 0x7fc00001u;

size_t Axis(const Input &input) {
  for (size_t axis = 0; axis < 3; ++axis) {
    if (input.Plane[axis] != 0) { return axis; }
  }
  return 3;
}

Vector Multiply(const Mat4f &matrix, const Vector &point) {
  Vector result{};
  for (size_t row = 0; row < 4; ++row) {
    for (size_t col = 0; col < 4; ++col) {
      result[row] += static_cast<double>(matrix.At(row, col)) * point[col];
    }
  }
  return result;
}

std::array<double, 3> Reference(const Input &input, size_t derivative) {
  const auto axis = Axis(input);
  Vector coordinate{Complex{input.Pixel[0]}, Complex{input.Pixel[1]}, Complex{1}, Complex{1}};
  coordinate[derivative] += Complex{0, kComplexStep};
  const auto near = Multiply(input.B, coordinate);
  coordinate[2] = 0;
  const auto far = Multiply(input.B, coordinate);
  const double height = input.Plane[3];
  const auto nearPlane = near[axis] - height * near[3];
  const auto farPlane = far[axis] - height * far[3];
  Vector point{};
  for (size_t i = 0; i < point.size(); ++i) { point[i] = farPlane * near[i] - nearPlane * far[i]; }
  std::array<double, 3> result{};
  const double pixelScale = (derivative == 0 ? 2.0 : -2.0) / input.Pixel[2 + derivative];
  for (size_t i = 0; i < result.size(); ++i) {
    result[i] = std::imag(point[i] / point[3]) / kComplexStep * pixelScale;
  }
  return result;
}

std::vector<Input> Controls() {
  std::vector<Input> result(kCapturedPlanes.begin(), kCapturedPlanes.end());
  for (const auto &input : kCapturedPlanes) {
    const auto tangent = (Axis(input) + 1) % 3;
    for (const float shift : {10000.f, 1000000.f}) {
      Input translated = input;
      for (size_t col = 0; col < 4; ++col) {
        translated.B.At(tangent, col) =
            std::fma(shift, input.B.At(3, col), input.B.At(tangent, col));
      }
      result.push_back(translated);
    }
  }
  const auto charts = result;
  for (const auto &input : charts) {
    for (const int exponent : {-64, -32, 32, 64}) {
      Input scaled = input;
      for (auto &value : scaled.B.Column) { value = std::ldexp(value, exponent); }
      result.push_back(scaled);
    }
  }
  return result;
}

void Compare(std::span<const float> values, std::span<const Input> inputs) {
  size_t projected = 0;
  size_t degenerate = 0;
  for (size_t i = 0; i < inputs.size(); ++i) {
    const auto &input = inputs[i];
    const auto axis = Axis(input);
    CHECK(axis < 3, "the actual captured normal names one original face axis");
    if (axis >= 3) { return; }
    const double incidence = static_cast<double>(input.B.At(axis, 2)) -
                             static_cast<double>(input.Plane[3]) * input.B.At(3, 2);
    const auto row = values.subspan(i * 8, 8);
    CHECK(row[7] == static_cast<float>(i), "every case writes its own complete indexed result");
    CHECK(row[3] == (incidence != 0 ? 1.f : 0.f),
          "exact camera-plane incidence rejects only zero image area, not nonzero adjacent floats");
    if (incidence == 0) {
      ++degenerate;
      CHECK(std::isnan(row[0]) && std::isnan(row[4]),
            "a degenerate plane never fabricates a zero UV footprint");
      continue;
    }
    ++projected;
    for (size_t derivative = 0; derivative < 2; ++derivative) {
      const auto expected = Reference(input, derivative);
      double squaredError = 0;
      double squaredNorm = 0;
      for (size_t component = 0; component < 3; ++component) {
        const float actual = row[derivative * 4 + component];
        CHECK(std::isfinite(actual) && std::isfinite(expected[component]),
              "actual plane differential is finite");
        squaredError += std::pow(static_cast<double>(actual) - expected[component], 2);
        squaredNorm += expected[component] * expected[component];
      }
      CHECK(std::sqrt(squaredError) / std::max(1.0, std::sqrt(squaredNorm)) < 2e-5,
            "the original plane Jacobian matches independent complex-step pixel rays at grazing "
            "and translated frames");
      CHECK(row[derivative * 4 + axis] == 0,
            "the normal derivative is exactly zero in the original local chart");
    }
  }
  CHECK(projected == 390 && degenerate == 45,
        "real captures, translated charts, homogeneous scales and nonzero adjacent planes are "
        "exercised");
}

void CheckDifferentials(Device &device) {
  const auto inputs = Controls();
  std::vector<uint32_t> words(inputs.size() * sizeof(Input) / sizeof(uint32_t));
  std::memcpy(words.data(), inputs.data(), inputs.size() * sizeof(Input));
  const auto pipeline =
      Require(device.CreateComputePipeline(Spirv(OUTSHINE_PRISM_DIFFERENTIAL_SHADER)));
  auto commands = Require(device.Begin());
  const auto source = Upload(device, *commands, words);
  const auto output = Upload(device, *commands, std::vector<uint32_t>(inputs.size() * 8, kPoison));
  const std::array bindings{Storage(0, 0, *source), Storage(1, 0, *output)};
  Require(commands->Dispatch(
      *pipeline, bindings, {.X = static_cast<uint32_t>((inputs.size() + 63) / 64)}));
  const auto download = Require(device.CreateBuffer({.Bytes = output->Description().Bytes,
                                                     .Use = BufferUse::CopyDestination,
                                                     .Placement = Memory::Readback}));
  Require(commands->Copy(*output, *download, {.Bytes = output->Description().Bytes}));
  const auto complete = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(complete, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "actual shader differentials finish before bounded readback");
  const auto mapped = Require(device.MapRead(*download, {.Bytes = download->Description().Bytes}));
  std::vector<float> values(mapped.size() / sizeof(float));
  std::memcpy(values.data(), mapped.data(), mapped.size());
  Require(device.Unmap(*download));
  Compare(values, inputs);
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  CheckDifferentials(*device);
  return Report();
}

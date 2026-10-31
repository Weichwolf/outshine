#include "Check.h"
#include "Readback.h"
#include "TransformMatrix.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr double kFrameTolerance = 64 * std::numeric_limits<float>::epsilon();

struct alignas(16) Input {
  Mat4f Model;
  std::array<float, 4> Normal{0, 0, 1, 1};
  std::array<float, 4> Tangent{1, 0, 0, 1};
  std::array<float, 4> Sampled{.2f, .6f, .8f, 0};
};

struct alignas(16) Output {
  std::array<float, 4> Mapped;
  std::array<float, 4> Tangent;
};

static_assert(sizeof(Input) == 112 && alignof(Input) == 16);
static_assert(offsetof(Input, Model) == 0 && offsetof(Input, Normal) == 64 &&
              offsetof(Input, Tangent) == 80 && offsetof(Input, Sampled) == 96);
static_assert(sizeof(Output) == 32 && alignof(Output) == 16);
static_assert(offsetof(Output, Mapped) == 0 && offsetof(Output, Tangent) == 16);

Vec3 Unit(const Vec3 &value) {
  return value * (1.0 / std::sqrt(Dot(value, value)));
}

std::vector<Input> Controls() {
  std::array<Mat4f, 8> models;
  models[1][0] = models[1][10] = .8f;
  models[1][2] = -.6f;
  models[1][8] = .6f;
  models[2][0] = 2;
  models[2][5] = 3;
  models[2][10] = 4;
  models[3] = models[2];
  models[3][0] = -2;
  models[4][4] = .4f;
  models[4][8] = .2f;
  models[4][9] = .25f;
  models[5] = models[4];
  models[5][0] = -1;
  models[6] = models[3];
  models[6][5] = -3;
  models[7] = models[1];
  for (size_t at = 0; at < 3; ++at) {
    models[7][at] *= -2;
    models[7][4 + at] *= 3;
    models[7][8 + at] *= 4;
  }
  std::vector<Input> inputs;
  for (const auto &model : models) {
    for (float orientation : {-1.f, 1.f}) {
      for (float side : {-1.f, 1.f}) {
        inputs.push_back(
            {.Model = model, .Normal = {0, 0, 1, side}, .Tangent = {1, 0, 0, orientation}});
      }
    }
  }
  return inputs;
}

void CheckFrame(const Input &input, const Output &output) {
  Mat4 model;
  Mat4 inverse;
  for (size_t at = 0; at < 16; ++at) { model[at] = input.Model[at]; }
  CHECK(InverseMatrix(model, inverse), "the independent affine reference is invertible");
  const Vec3 normal = Unit({{inverse[2], inverse[6], inverse[10]}});
  const Vec3 rawTangent = model.TransformDirection({{1, 0, 0}});
  const Vec3 tangent = Unit(rawTangent - normal * Dot(normal, rawTangent));
  const Vec3 rawBitangent = model.TransformDirection({{0, input.Tangent[3], 0}});
  const Vec3 bitangent = Unit(rawBitangent - normal * Dot(normal, rawBitangent) -
                              tangent * Dot(tangent, rawBitangent));
  const double orientation = Dot(Cross(normal, tangent), bitangent) < 0 ? -1 : 1;
  CHECK(output.Tangent[3] == orientation,
        "instance orientation preserves the independently transformed bitangent");
  const Vec3 mapped = Unit(tangent * static_cast<double>(input.Sampled[0]) +
                           bitangent * static_cast<double>(input.Sampled[1]) +
                           normal * static_cast<double>(input.Sampled[2])) *
                      static_cast<double>(input.Normal[3]);
  const Vec3 expectedTangent = Unit(rawTangent);
  for (size_t axis = 0; axis < 3; ++axis) {
    CHECK(std::abs(output.Mapped[axis] - mapped[axis]) <= kFrameTolerance,
          "normal-map direction matches the independent transformed frame on either side");
    CHECK(std::abs(output.Tangent[axis] - expectedTangent[axis]) <= kFrameTolerance,
          "the existing transformed tangent direction is preserved");
  }
  CHECK(output.Mapped[3] == 1, "every native result is completely written");
}

void CheckFrames(Device &device) {
  const auto inputs = Controls();
  std::vector<uint32_t> words(inputs.size() * sizeof(Input) / sizeof(uint32_t));
  std::memcpy(words.data(), inputs.data(), inputs.size() * sizeof(Input));
  const auto pipeline = Require(device.CreateComputePipeline(Spirv(OUTSHINE_SURFACE_FRAME_SHADER)));
  auto commands = Require(device.Begin());
  const auto source = Upload(device, *commands, words);
  const auto output =
      Upload(device, *commands, std::vector<uint32_t>(inputs.size() * 8, 0x7fc00001u));
  const std::array bindings{Storage(0, 0, *source), Storage(1, 0, *output)};
  Require(commands->Dispatch(*pipeline, bindings, {.X = 1}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds{10})) == CompletionStatus::Ready,
        "surface frames complete within the diagnostic bound");
  Readback read;
  CHECK(read.FromBuffer(&device, output.get(), inputs.size() * sizeof(Output)) == ReadState::Ready,
        "the actual native surface frames are readable");
  if (read.Rows() == nullptr) { return; }
  for (size_t at = 0; at < inputs.size(); ++at) {
    Output value;
    std::memcpy(&value, read.Rows() + at * sizeof(Output), sizeof(Output));
    CheckFrame(inputs[at], value);
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  CheckFrames(*device);
  return Report();
}

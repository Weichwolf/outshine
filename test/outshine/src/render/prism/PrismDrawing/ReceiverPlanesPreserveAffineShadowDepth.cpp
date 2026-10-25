#include "Check.h"
#include "math/Mat4.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr double kQuarter = .25, kThreeQuarters = .75;
constexpr float kLowDepth = .48f, kHighDepth = .52f;
constexpr float kLargeScale = 1e30f, kMiddleScale = 1e20f, kSmallScale = 1e-20f;
constexpr size_t kResultFloats = 8, kInputBytes = 144;
constexpr uint32_t kPoisonNaN = 0x7fc00001u;
constexpr float kReverseDepthScale = -.125f;

struct Control {
  Mat4f Model;
  Mat4f Light;
  std::array<std::array<double, 2>, 3> Gradients{};
  std::array<bool, 3> Parallel{true, true, false};
  bool Exact = true;
};

struct alignas(16) Input {
  Mat4f Model;
  Mat4f Light;
  std::array<uint32_t, 4> Meta{};
};

static_assert(sizeof(Input) == kInputBytes && alignof(Input) == 16);
static_assert(offsetof(Input, Model) == 0 && offsetof(Input, Light) == 64 &&
              offsetof(Input, Meta) == 128);

std::vector<Control> Controls() {
  std::vector<Control> rows(1);
  Control shear;
  shear.Model[2] = .5f;
  shear.Model[6] = static_cast<float>(kQuarter);
  shear.Gradients[2] = {1, -.5};
  rows.push_back(shear);
  Control stretched = shear;
  stretched.Model[0] = 2;
  stretched.Model[2] = 2;
  stretched.Model[5] = .5f;
  stretched.Model[6] = 1;
  stretched.Model[10] = 4;
  stretched.Gradients[2] = {2, -4};
  rows.push_back(stretched);
  Control mirrored = shear;
  mirrored.Model[0] = -1;
  mirrored.Gradients[2] = {-1, -.5};
  rows.push_back(mirrored);
  Control reverse = stretched;
  reverse.Light[0] = .5f;
  reverse.Light[5] = static_cast<float>(kQuarter);
  reverse.Light[10] = kReverseDepthScale;
  reverse.Gradients[2] = {-.5, 2};
  rows.push_back(reverse);
  Control oblique;
  oblique.Light[2] = -1;
  oblique.Light[8] = 1;
  oblique.Gradients[0] = {2, 0};
  oblique.Gradients[2] = {-2, 0};
  oblique.Parallel = {false, true, false};
  rows.push_back(oblique);
  Control grazing = shear;
  grazing.Model[2] = kMiddleScale;
  grazing.Gradients[2] = {2 * static_cast<double>(kMiddleScale), -.5};
  grazing.Exact = false;
  rows.push_back(grazing);
  Control extreme = oblique;
  extreme.Model[0] = kLargeScale;
  extreme.Model[5] = kMiddleScale;
  extreme.Model[10] = kSmallScale;
  extreme.Exact = false;
  rows.push_back(extreme);
  return rows;
}

double Visibility(const std::array<double, 2> &gradient) {
  const std::array depth{kLowDepth, kHighDepth, kLowDepth, kHighDepth};
  std::array<double, 4> lit{};
  for (size_t at = 0; at < lit.size(); ++at) {
    const double x = (static_cast<double>(at % 2) - kQuarter) / 16;
    const size_t row = at / 2;
    const double y = (static_cast<double>(row) - kThreeQuarters) / 16;
    lit[at] = .5 + gradient[0] * x + gradient[1] * y >= static_cast<double>(depth[at]) ? 1 : 0;
  }
  return (lit[0] * kThreeQuarters + lit[1] * kQuarter) * kQuarter +
         (lit[2] * kThreeQuarters + lit[3] * kQuarter) * kThreeQuarters;
}

void CheckPlane(std::span<const float> values, const Control &control, size_t axis) {
  for (const float component : values) {
    CHECK(std::isfinite(component), "receiver output is finite at large affine scales");
  }
  CHECK(values[3] == (control.Parallel[axis] ? 0.f : 1.f),
        "exactly parallel faces retain the point-depth fallback");
  CHECK(values[5] == static_cast<float>(Visibility(control.Gradients[axis])),
        "four shadow taps match the independent affine plane equation");
  if (!control.Exact) { return; }
  const auto &gradient = control.Gradients[axis];
  const double scale = std::max({std::abs(gradient[0]), std::abs(gradient[1]), 1.0});
  const std::array expected{control.Parallel[axis] ? 0.f : static_cast<float>(gradient[0] / scale),
                            control.Parallel[axis] ? 0.f : static_cast<float>(gradient[1] / scale),
                            control.Parallel[axis] ? 0.f : static_cast<float>(1.0 / scale)};
  CHECK(std::equal(expected.begin(), expected.end(), values.begin()),
        "dyadic shear, reflection and reverse-depth controls have exact receiver coefficients");
}

void CheckReceivers(Device &device) {
  const auto controls = Controls();
  std::vector<Input> inputs;
  for (const auto &control : controls) {
    for (uint32_t face = 0; face < 6; ++face) {
      inputs.push_back({.Model = control.Model,
                        .Light = control.Light,
                        .Meta = {face / 2, face % 2, static_cast<uint32_t>(inputs.size()), 0}});
    }
  }
  std::vector<uint32_t> words(inputs.size() * sizeof(Input) / sizeof(uint32_t));
  std::memcpy(words.data(), inputs.data(), inputs.size() * sizeof(Input));
  const auto pipeline =
      Require(device.CreateComputePipeline(Spirv(OUTSHINE_PRISM_RECEIVER_SHADER)));
  auto commands = Require(device.Begin());
  const auto source = Upload(device, *commands, words);
  auto output =
      Upload(device, *commands, std::vector<uint32_t>(inputs.size() * kResultFloats, kPoisonNaN));
  const std::array bindings{Storage(0, 0, *source), Storage(1, 0, *output)};
  Require(commands->Dispatch(*pipeline, bindings, {.X = 1}));
  auto download = Require(device.CreateBuffer({.Bytes = output->Description().Bytes,
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  Require(commands->Copy(*output, *download, {.Bytes = output->Description().Bytes}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds{10})) == CompletionStatus::Ready,
        "native receiver controls complete within the diagnostic bound");
  const auto mapped = Require(device.MapRead(*download, {.Bytes = download->Description().Bytes}));
  std::vector<float> values(mapped.size() / sizeof(float));
  std::memcpy(values.data(), mapped.data(), mapped.size());
  Require(device.Unmap(*download));
  for (size_t at = 0; at < inputs.size(); ++at) {
    const auto row = std::span(values).subspan(at * kResultFloats, kResultFloats);
    CHECK(row[4] == static_cast<float>(at) && row[6] == 0 && row[7] == 1,
          "every case writes its own complete result");
    CheckPlane(row, controls[at / 6], (at % 6) / 2);
    if (at % 2 == 1) {
      const auto previous = std::span(values).subspan((at - 1) * kResultFloats, 4);
      CHECK(std::equal(previous.begin(), previous.end(), row.begin()),
            "opposite faces share the same geometric receiver plane");
    }
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  CheckReceivers(*device);
  return Report();
}

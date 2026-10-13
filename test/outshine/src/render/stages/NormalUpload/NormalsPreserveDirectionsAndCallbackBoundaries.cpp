#include "Check.h"
#include "NormalUpload.h"
#include "math/Octahedral.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::array<float, 18> normals{
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, -1, -1, -1, -1, 0.2f, -0.6f, -0.4f};

void Callback(const void *source, float *into, uint32_t floats) {
  CHECK(floats == normals.size(), "callback receives the full Float3 input count");
  std::memcpy(into, source, floats * sizeof(float));
}

void Verify(outshine::Render::SubjectStream source) {
  using namespace outshine;
  using namespace outshine::Render;
  std::array<float, 8> storage{};
  storage.front() = 123.0f;
  storage.back() = 456.0f;
  const NormalUpload upload{.Source = source};
  NormalUpload::Write(&upload, storage.data() + 1, 6);
  CHECK(storage.front() == 123.0f && storage.back() == 456.0f,
        "encoding touches exactly one word per normal");
  for (size_t at = 0; at < 6; ++at) {
    uint32_t word = 0;
    std::memcpy(&word, storage.data() + at + 1, sizeof(word));
    const auto decoded = OctUnfolded(UnpackedPair(word));
    const double length = std::sqrt(double{normals[at * 3]} * normals[at * 3] +
                                    double{normals[at * 3 + 1]} * normals[at * 3 + 1] +
                                    double{normals[at * 3 + 2]} * normals[at * 3 + 2]);
    double error = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      const double delta = decoded[axis] - normals[at * 3 + axis] / length;
      error += delta * delta;
    }
    CHECK(std::sqrt(error) < 1e-4, "both hemispheres retain direction within the packing bound");
  }
}
}

int main() {
  Verify({.From = normals.data()});
  Verify({.Writes = Callback, .Carrying = normals.data()});
  return outshine::Test::Report();
}

#ifndef OUTSHINE_EXPERIMENT_UNCERTAIN_PATCH_H
#define OUTSHINE_EXPERIMENT_UNCERTAIN_PATCH_H
#include "FixedPatch.h"
#include "vertex_projection/Envelope.h"
#include <array>
#include <cstdint>
#include <expected>

namespace FixedCoverage {
struct UncertainQuad {
  std::array<std::array<Projection::Interval, 4>, 4> Clip{};
  int FrontWindowOrientation = -1;
  bool Opaque = false;
  bool Complete = false;
};

struct HalfPlane {
  Exact A;
  Exact B;
  Exact C;
};

struct UncertainOracle {
  std::array<HalfPlane, 64> Edges;
  Exact FarDepth;
  ExactPoint Min;
  ExactPoint Max;
};

[[nodiscard]] std::expected<Projected, Unknown>
Project(const UncertainQuad &source, uint32_t width, uint32_t height);
[[nodiscard]] std::expected<FixedPatch, Unknown>
Prepare(const UncertainQuad &source, uint32_t width, uint32_t height);
[[nodiscard]] std::expected<FixedPatch, Unknown>
PrepareInteger(const UncertainQuad &source, uint32_t width, uint32_t height);
[[nodiscard]] std::expected<UncertainOracle, Unknown>
MakeOracle(const UncertainQuad &source, uint32_t width, uint32_t height);
[[nodiscard]] bool
Covers(const UncertainOracle &source, const std::array<uint32_t, 2> &pixel, float depth);
}
#endif

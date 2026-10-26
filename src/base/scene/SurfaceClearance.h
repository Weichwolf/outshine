#ifndef OUTSHINE_BASE_SCENE_SURFACECLEARANCE_H
#define OUTSHINE_BASE_SCENE_SURFACECLEARANCE_H

#include "Vec2.h"
#include "Vec3.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine {

struct HeightSurfaceView {

  std::span<const Vec3> Vertices;

  std::span<const std::array<uint32_t, 3>> Triangles;
};

struct SurfaceClearanceLimits {

  static constexpr size_t DefaultMaxCandidateTests = 1000000;

  size_t MaxVertices = size_t{3} * 65536;

  size_t MaxFaces = 8192;

  size_t MaxCandidateTests = DefaultMaxCandidateTests;

  size_t MaxPairs = 65536;

  std::chrono::steady_clock::time_point Deadline = std::chrono::steady_clock::time_point::max();
};

enum class SurfaceClearanceError {
  InvalidInput,
  InputBudget,
  CandidateBudget,
  PairBudget,
  Deadline,
  Unrepresentable,
  NoOverlap,
  Backend
};

struct SurfaceClearanceFailure {

  SurfaceClearanceError Error;

  size_t CandidateTests = 0;

  size_t Pairs = 0;

  size_t Overlaps = 0;
};

struct SurfaceClearance {

  double MinimumM = 0;

  Vec2 Witness;

  size_t LowerFace = 0;

  size_t UpperFace = 0;

  size_t CandidateTests = 0;

  size_t Pairs = 0;

  size_t Overlaps = 0;
};

[[nodiscard]] std::expected<SurfaceClearance, SurfaceClearanceFailure> MeasureSurfaceClearance(
    HeightSurfaceView lower, HeightSurfaceView upper, SurfaceClearanceLimits limits = {});

}
#endif

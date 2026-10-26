#ifndef OUTSHINE_BASE_SCENE_SURFACESAMPLER_H
#define OUTSHINE_BASE_SCENE_SURFACESAMPLER_H

#include "Vec2.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <vector>

namespace outshine {

enum class SurfaceSamplerError { InvalidInput, InputBudget, Unrepresentable, Backend };

enum class SurfaceContactStatus { Contact, Outside, Ambiguous, CandidateBudget };

struct SurfaceSamplerLimits {

  static constexpr double DefaultXyToleranceM = 1e-8;

  static constexpr double DefaultHeightToleranceM = 1e-7;

  size_t MaxVertices = size_t{3} * 65536;

  size_t MaxFaces = 65536;

  double XyToleranceM = DefaultXyToleranceM;

  double HeightToleranceM = DefaultHeightToleranceM;
};

struct SurfaceQueryLimits {

  static constexpr size_t DefaultMaxCandidates = 1000000;

  size_t MaxQueries = 65536;

  size_t MaxCandidates = DefaultMaxCandidates;
};

struct SurfaceContact {

  Vec3 PositionM;

  Vec3 Normal;

  int64_t FaceIndex = -1;

  SurfaceContactStatus Status = SurfaceContactStatus::Outside;
};

struct SurfaceSamples {

  std::vector<SurfaceContact> Contacts;

  size_t Candidates = 0;

  size_t Batches = 0;
};

class SurfaceSampler {
public:
  SurfaceSampler(SurfaceSampler &&) noexcept;

  SurfaceSampler &operator=(SurfaceSampler &&) noexcept;
  ~SurfaceSampler();

  [[nodiscard]] static std::expected<SurfaceSampler, SurfaceSamplerError>
  prepare(std::span<const Vec3> vertices,
          std::span<const std::array<uint32_t, 3>> triangles,
          SurfaceSamplerLimits limits = {});

  [[nodiscard]] std::expected<SurfaceSamples, SurfaceSamplerError>
  sample(std::span<const Vec2> xy, SurfaceQueryLimits limits = {}) const;

private:
  class State;
  explicit SurfaceSampler(std::unique_ptr<State> state);
  std::unique_ptr<State> State_;
};

}
#endif

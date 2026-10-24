#ifndef OUTSHINE_BASE_SPATIAL_HEIGHTBANDMESH_H
#define OUTSHINE_BASE_SPATIAL_HEIGHTBANDMESH_H

#include "PlanarMesh.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine {
struct HeightLevelRange {
  uint32_t First = 0, Count = 0;
};

struct HeightBandInput {
  std::span<const std::array<double, 3>> Points;
  std::span<const PlanarFace> Faces;
  std::span<const HeightLevelRange> LevelRanges;
  std::span<const double> Levels;
};

struct HeightBandBudget {
  size_t InputPoints = 4096;
  size_t InputFaces = 8192;
  size_t Points = 65536;
  size_t Faces = 8192;
  size_t Levels = 4096;
  size_t EdgeVisits = 262144;
};

struct HeightBandWork {
  size_t LevelChecks = 0;
  size_t BandVisits = 0;
  size_t SkippedBands = 0;
  size_t SearchComparisons = 0;
  size_t EdgeVisits = 0;
  size_t IntersectionQueries = 0;
  size_t NewIntersections = 0;
  size_t RemovedZeroFaces = 0;
  size_t CopiedInputBytes = 0;
};

struct HeightBandMesh {
  std::vector<std::array<double, 3>> Points;
  std::vector<PlanarFace> Faces;
  std::vector<uint32_t> FaceParents;
  std::vector<int32_t> LevelIndices;
  HeightBandWork Work;
};

enum class HeightBandError : uint8_t { InvalidInput, CapacityExceeded, Canceled, Unrepresentable };

[[nodiscard]] std::expected<HeightBandMesh, HeightBandError> CutHeightBands(
    const HeightBandInput &input, HeightBandBudget budget = {}, const std::stop_token &stop = {});
}
#endif

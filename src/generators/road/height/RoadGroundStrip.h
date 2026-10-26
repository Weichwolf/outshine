#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDSTRIP_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDSTRIP_H

#include "RoadPortFieldFit.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
enum class RoadGroundError : uint8_t {
  InvalidAxis,
  UnsupportedRecipe,
  FoldedStrip,
  MissingTerrain,
  InvalidGeometry,
  CapacityExceeded,
  Canceled
};

struct RoadGroundBudget {
  size_t AxisPoints = 256;
  size_t Controls = 256;
  size_t TerrainSamples = 4096;
  size_t ContainerBytes = 4194304;
};

struct RoadGroundWork {
  size_t AxisPoints = 0;
  size_t Controls = 0;
  size_t TerrainSamples = 0;
  size_t Faces = 0;
  size_t MergedPlanes = 0;
  size_t ContainerCapacityBytes = 0;
};

struct RoadGroundStrip {
  LongitudeLatitude Anchor;
  std::vector<LongitudeLatitude> TerrainQueries;
  std::vector<std::array<uint32_t, 3>> Triangles;
  std::vector<uint32_t> FacePatches;
  std::vector<std::array<double, 4>> GradeFrames;
  std::vector<uint32_t> Boundaries;
  RoadGroundWork Work;
};

struct RoadGroundField {
  LongitudeLatitude Anchor;
  std::vector<EastNorthUp> Vertices;
  std::vector<std::array<uint32_t, 3>> Triangles;
  std::vector<RoadPortField::Plane> Planes;
  std::vector<RoadPortField::Patch> Patches;
  std::vector<RoadPortPrior> Priors;
  std::vector<RoadPortFace> Faces;
  RoadGroundWork Work;

  [[nodiscard]] RoadPortFieldFitInput FitInput(double maximumPriorChangeM) const noexcept;
};

[[nodiscard]] std::expected<RoadGroundStrip, RoadGroundError>
PrepareRoadGroundStrip(std::span<const LongitudeLatitude> geographic,
                       double widthM,
                       double maximumGradient,
                       RoadGroundBudget budget = {},
                       const std::stop_token &stop = {});

[[nodiscard]] std::expected<RoadGroundField, RoadGroundError>
BindRoadGroundField(const RoadGroundStrip &strip,
                    std::span<const double> modelHeightsM,
                    RoadGroundBudget budget = {},
                    const std::stop_token &stop = {});
}
#endif

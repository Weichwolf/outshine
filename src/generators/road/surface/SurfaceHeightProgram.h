#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEHEIGHTPROGRAM_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEHEIGHTPROGRAM_H

#include "SurfaceHeightTopology.h"
#include "HeightProgram.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
using SurfaceHeightPoint = std::array<double, 3>;

struct SurfaceHeightRecipe {
  double WidthM = 0, MaximumGrade = 0;
  bool Steps = false;
  std::vector<PlanarPoint> Axis;
  std::vector<SurfaceHeightPoint> Prior;
  std::vector<std::array<PlanarPoint, 2>> Sections;
  PlanarDomain Domain;
};

struct SurfaceHeightJunction {
  PlanarPoint Origin{};
  std::vector<uint32_t> Vertices;
};

struct SurfaceHeightFlight {
  uint32_t Source = 0, Start = 0, End = 0;
  int Direction = 1;
  double CutStart = 0, CutEnd = 0;
  std::vector<double> Stations;
};

struct SurfaceHeightContact {
  uint32_t Source = 0;
  std::vector<SurfaceHeightPoint> Points;
};

struct SurfaceHeightOrder {
  uint32_t Lower = 0, Upper = 0;
};

struct SurfaceHeightRequest {
  const SurfaceHeightTopology &Topology;
  std::span<const SurfaceHeightRecipe> Sources;
  std::span<const SurfaceHeightJunction> Ports;
  std::span<const SurfaceHeightFlight> Flights;
  std::span<const SurfaceHeightContact> Contacts;
  std::span<const SurfaceHeightOrder> Orders;
  double MaximumAdjustmentM = 0, MaximumCrossfall = 0;
};

struct SurfaceHeightProgramBudget {
  static constexpr size_t kDefaultGeometryCandidates = 200000;
  static constexpr size_t kDefaultPriorTests = 1000000;
  static constexpr size_t kDefaultProjectionProducts = 2000000;
  size_t Variables = 4096;
  size_t Sources = 64;
  size_t Ports = 64;
  size_t Flights = 16;
  size_t Coordinates = 65536;
  size_t Faces = 8192;
  size_t Conditions = RoadProfiles::HeightFitBudget::kDefaultConditions;
  size_t Nonzeros = RoadProfiles::HeightFitBudget::kDefaultNonzeros;
  size_t GeometryCandidates = kDefaultGeometryCandidates;
  size_t ContactSamples = 32768;
  size_t PriorTests = kDefaultPriorTests;
  size_t ProjectionProducts = kDefaultProjectionProducts;
  size_t SectionTests = kDefaultPriorTests;
};

struct SurfaceHeightProgramWork {
  size_t ContactSamples = 0;
  size_t GeometryQueries = 0;
  size_t CandidateFaces = 0;
  size_t OverlapRows = 0;
  size_t GradientRows = 0;
  size_t PriorTests = 0;
  size_t ProjectionProducts = 0;
  size_t SectionTests = 0;
};

struct SurfaceHeightProgram {
  RoadProfiles::HeightProgram Program;
  SparseMatrix<int> Projection;
  double DatumM = 0;
  size_t PointCount = 0;
  SurfaceHeightProgramWork Work;
};

enum class SurfaceHeightProgramError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  LibraryFailure,
  ConflictingOwners
};

[[nodiscard]] std::expected<SurfaceHeightProgram, SurfaceHeightProgramError>
BuildSurfaceHeightProgram(const SurfaceHeightRequest &request,
                          SurfaceHeightProgramBudget budget = {},
                          const std::stop_token &stop = {});
}
#endif

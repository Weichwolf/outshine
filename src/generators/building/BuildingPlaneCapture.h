#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGPLANECAPTURE_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGPLANECAPTURE_H

#include "BuildingSurface.h"
#include "PlanHierarchy.h"
#include "ImpostorViewFrame.h"
#include "StructureMesher.h"
#include "Vec3.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
class BuildingPlaneCapture {
public:
  struct View {
    Vec3 Centre, TowardEye;
    double HalfExtentM = 0;
    uint32_t Pixels = 0;
    std::optional<ImpostorViewFrame> Frame = std::nullopt;
  };

  struct Budget {
    uint64_t Samples = 0, SourceTests = 0;
  };

  enum class ErrorKind {
    InvalidInput,
    WorkBudgetExceeded,
    SourceUnavailable,
    UnsupportedSource,
    Cancelled
  };

  struct Error {
    ErrorKind Kind;
    std::optional<uint32_t> Source = std::nullopt;
  };

  struct Result {
    Raised Mesh;
    std::vector<uint32_t> Sources;
    uint64_t Rays = 0, Covered = 0, SourceTests = 0;
  };

  // Sources must outlive the capture; sampled opaque exteriors carry no World error/reuse bound.
  [[nodiscard]] static std::expected<BuildingPlaneCapture, Error>
  Prepare(std::span<const BuildingSurface *const> sources);
  [[nodiscard]] std::expected<Result, Error>
  Capture(const View &view, Budget budget, std::stop_token stopping = {}) const;

private:
  BuildingPlaneCapture(std::vector<const BuildingSurface *> sources, PlanHierarchy hierarchy)
      : Sources_(std::move(sources)), Hierarchy_(std::move(hierarchy)) {}

  std::vector<const BuildingSurface *> Sources_;
  PlanHierarchy Hierarchy_;
};
}
#endif

#ifndef OUTSHINE_TEST_BUILDING_SELECTION_OBSERVER_H
#define OUTSHINE_TEST_BUILDING_SELECTION_OBSERVER_H

#include "BuildingMesh.h"
#include "PreparedStructureTile.h"
#include "BuildingScratch.h"
#include "building_capture_inputs.h"
#include <nlohmann/json.hpp>
#include <cstddef>
#include <span>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace outshine::Test {
enum class BuildingSelectionMode { Projected, FullView };

class BuildingSelectionObserver final : public StructureMesher {
public:
  explicit BuildingSelectionObserver(const Generators::PreparedStructureTile &base,
                                     size_t captureTriangleLimit = 0,
                                     BuildingSelectionMode mode = BuildingSelectionMode::Projected)
      : CaptureTriangleLimit_(captureTriangleLimit), Mode_(mode) {
    for (size_t index = 0; index < base.Surfaces.size(); ++index) {
      Sources_.emplace(&base.Surfaces[index], index);
    }
  }

  std::unique_ptr<MeshScratch> Scratch() const override { return Native_.Scratch(); }

  bool HasSurfaceProjection() const noexcept override {
    return Mode_ == BuildingSelectionMode::Projected;
  }

  std::optional<Box> SourceEnvelopeBounds(const StructurePlan &plan,
                                          MeshScratch &scratch) const noexcept override {
    return Native_.SourceEnvelopeBounds(plan, scratch);
  }

  std::optional<double> ShellSurfaceErrorM(const StructurePlan &plan,
                                           MeshScratch &scratch) const noexcept override {
    return Native_.ShellSurfaceErrorM(plan, scratch);
  }

  std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    const size_t wallFirst = into.WallRun.size();
    const size_t roofFirst = into.RoofRun.size();
    auto result = Native_.Mesh(plan, scratch, into);
    Meshes_.push_back(
        {{"source", SourceOf(plan)},
         {"detail", static_cast<unsigned>(plan.Coarseness)},
         {"openings", plan.RecessedOpenings},
         {"accepted", result.has_value()},
         {"error", result ? nlohmann::json(nullptr) : nlohmann::json(Describe(result.error()))}});
    if (Mode_ == BuildingSelectionMode::FullView) {
      auto &row = Meshes_.back();
      row["wall_index_range"] = {wallFirst, into.WallRun.size() - wallFirst};
      row["roof_index_range"] = {roofFirst, into.RoofRun.size() - roofFirst};
      row["part"] = nullptr;
    }
    return result;
  }

  std::expected<ProjectedStructureMesh, StructureMeshError>
  Project(std::span<const StructurePlan> plans,
          const Vec3 &eye,
          ProjectedErrorBudget projection,
          MeshScratch &scratch) const override {
    auto members = nlohmann::json::array();
    for (const auto &plan : plans) {
      members.push_back({{"source", SourceOf(plan)},
                         {"detail", static_cast<unsigned>(plan.Coarseness)},
                         {"openings", plan.RecessedOpenings}});
    }
    Generators::BuildingSurfaceCaptureRecord record;
    auto *nativeScratch = dynamic_cast<Generators::BuildingScratch *>(&scratch);
    if (CaptureTriangleLimit_ != 0 && nativeScratch == nullptr) {
      return std::unexpected(StructureMeshError::IncompatibleScratch);
    }
    auto result =
        CaptureTriangleLimit_ == 0
            ? Native_.Project(plans, eye, projection, scratch)
            : Generators::CaptureBuildingSurfaces(plans, eye, projection, *nativeScratch, &record);
    nlohmann::json row{{"members", std::move(members)}, {"accepted", result.has_value()}};
    if (result && CaptureTriangleLimit_ != 0) {
      auto captured = BuildingCaptureInputs(
          plans, eye, projection, record, result->Mesh, CaptureTriangleLimit_);
      if (captured) {
        row["capture"] = std::move(*captured);
        ++CapturedCalls_;
      } else {
        row["capture_error"] = captured.error();
        CaptureError_ = captured.error();
      }
    }
    Projections_.push_back(std::move(row));
    return result;
  }

  nlohmann::json SourceOf(const StructurePlan &plan) const {
    if (plan.Prepared == nullptr) { return nullptr; }
    const auto found = Sources_.find(plan.Prepared);
    return found == Sources_.end() ? nlohmann::json("unrecognized") : nlohmann::json(found->second);
  }

  nlohmann::json TakeMeshes() { return std::move(Meshes_); }

  nlohmann::json TakeProjections() { return std::move(Projections_); }

  bool CaptureComplete() const noexcept { return !CaptureError_.has_value(); }

  size_t CapturedCalls() const noexcept { return CapturedCalls_; }

private:
  Generators::BuildingMesh Native_;
  mutable nlohmann::json Meshes_ = nlohmann::json::array();
  mutable nlohmann::json Projections_ = nlohmann::json::array();
  size_t CaptureTriangleLimit_ = 0;
  BuildingSelectionMode Mode_ = BuildingSelectionMode::Projected;
  mutable size_t CapturedCalls_ = 0;
  mutable std::optional<std::string> CaptureError_;

  std::unordered_map<const Generators::BuildingSurface *, size_t> Sources_;
};
}

#endif

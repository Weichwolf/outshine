#ifndef OUTSHINE_TEST_BUILDING_SELECTION_OBSERVER_H
#define OUTSHINE_TEST_BUILDING_SELECTION_OBSERVER_H

#include "BuildingMesh.h"
#include "PreparedStructureTile.h"
#include <nlohmann/json.hpp>
#include <cstddef>
#include <span>
#include <unordered_map>
#include <utility>

namespace outshine::Test {
class BuildingSelectionObserver final : public StructureMesher {
public:
  explicit BuildingSelectionObserver(const Generators::PreparedStructureTile &base) {
    for (size_t index = 0; index < base.Surfaces.size(); ++index) {
      Sources_.emplace(&base.Surfaces[index], index);
    }
  }

  std::unique_ptr<MeshScratch> Scratch() const override { return Native_.Scratch(); }

  bool HasSurfaceProjection() const noexcept override { return true; }

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
    auto result = Native_.Mesh(plan, scratch, into);
    Meshes_.push_back(
        {{"source", SourceOf(plan)},
         {"detail", static_cast<unsigned>(plan.Coarseness)},
         {"openings", plan.RecessedOpenings},
         {"accepted", result.has_value()},
         {"error", result ? nlohmann::json(nullptr) : nlohmann::json(Describe(result.error()))}});
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
    auto result = Native_.Project(plans, eye, projection, scratch);
    Projections_.push_back({{"members", std::move(members)}, {"accepted", result.has_value()}});
    return result;
  }

  nlohmann::json SourceOf(const StructurePlan &plan) const {
    if (plan.Prepared == nullptr) { return nullptr; }
    const auto found = Sources_.find(plan.Prepared);
    return found == Sources_.end() ? nlohmann::json("unrecognized") : nlohmann::json(found->second);
  }

  nlohmann::json TakeMeshes() { return std::move(Meshes_); }

  nlohmann::json TakeProjections() { return std::move(Projections_); }

private:
  Generators::BuildingMesh Native_;
  mutable nlohmann::json Meshes_ = nlohmann::json::array();
  mutable nlohmann::json Projections_ = nlohmann::json::array();

  std::unordered_map<const Generators::BuildingSurface *, size_t> Sources_;
};
}

#endif

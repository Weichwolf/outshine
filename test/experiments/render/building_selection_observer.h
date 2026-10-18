#pragma once

#include "BuildingMesh.h"
#include "PreparedStructureTile.h"
#include <nlohmann/json.hpp>
#include <cstddef>
#include <span>
#include <unordered_map>

namespace outshine::Test {
class BuildingSelectionObserver final : public StructureMesher {
public:
  explicit BuildingSelectionObserver(const Generators::PreparedStructureTile &base) {
    for (size_t index = 0; index < base.Surfaces.size(); ++index) {
      Sources.emplace(&base.Surfaces[index], index);
    }
  }

  std::unique_ptr<MeshScratch> Scratch() const override { return Native.Scratch(); }

  bool HasSurfaceProjection() const noexcept override { return true; }

  std::optional<Box> SourceEnvelopeBounds(const StructurePlan &plan,
                                          MeshScratch &scratch) const noexcept override {
    return Native.SourceEnvelopeBounds(plan, scratch);
  }

  std::optional<double> ShellSurfaceErrorM(const StructurePlan &plan,
                                           MeshScratch &scratch) const noexcept override {
    return Native.ShellSurfaceErrorM(plan, scratch);
  }

  std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    auto result = Native.Mesh(plan, scratch, into);
    Meshes.push_back({{"source", SourceOf(plan)},
                      {"detail", static_cast<unsigned>(plan.Coarseness)},
                      {"openings", plan.RecessedOpenings},
                      {"accepted", result.has_value()}});
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
    auto result = Native.Project(plans, eye, projection, scratch);
    Projections.push_back({{"members", std::move(members)}, {"accepted", result.has_value()}});
    return result;
  }

  nlohmann::json SourceOf(const StructurePlan &plan) const {
    if (!plan.Prepared) { return nullptr; }
    const auto found = Sources.find(plan.Prepared);
    return found == Sources.end() ? nlohmann::json("unrecognized") : nlohmann::json(found->second);
  }

  Generators::BuildingMesh Native;
  mutable nlohmann::json Meshes = nlohmann::json::array();
  mutable nlohmann::json Projections = nlohmann::json::array();

private:
  std::unordered_map<const Generators::BuildingSurface *, size_t> Sources;
};
}

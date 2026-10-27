#ifndef OUTSHINE_RENDER_MESHCLUSTERBUILDJOB_H
#define OUTSHINE_RENDER_MESHCLUSTERBUILDJOB_H

#include "ClusterCook.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace outshine {
struct Material;
}

namespace outshine::Render {
struct RenderGeometryStorage;
struct RenderGeometryPart;

class MeshClusterBuildJob {
public:
  MeshClusterBuildJob(RenderGeometryStorage &into, std::span<const Material> surfaces) noexcept;

  [[nodiscard]] std::expected<bool, ClusterError> Advance(size_t itemsMost);

  [[nodiscard]] bool Ready() const noexcept;

private:
  enum class Stage { Layout, Plan, Part, Rebase, Cook, Indices, Clusters, Complete, Failed };
  enum class Flow { Continue, Yield, Complete };

  [[nodiscard]] ClusterError Fail(ClusterError error) noexcept;
  [[nodiscard]] bool KeepsClusters(const RenderGeometryPart &part) const;
  [[nodiscard]] std::expected<Flow, ClusterError> PlanParts(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvanceStage(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvanceWork(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> BeginsPart();
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesRebase(size_t &itemsLeft);
  [[nodiscard]] std::expected<Flow, ClusterError> AdvancesCook(size_t &itemsLeft);
  [[nodiscard]] Flow PublishIndices(size_t &itemsLeft);
  [[nodiscard]] Flow PublishClusters(size_t &itemsLeft);
  void Completes() noexcept;

  RenderGeometryStorage *Into_ = nullptr;
  std::span<const Material> Surfaces_;
  std::optional<ClusterCookJob> Cook_;
  ClusteredMesh Built_;
  std::vector<uint32_t> LocalIndices_;
  Stage Stage_ = Stage::Layout;
  ClusterError Failure_ = ClusterError::InvalidLayout;
  size_t Part_ = 0;
  size_t Cursor_ = 0;
  size_t RootClusters_ = 0;
  size_t PlannedClusters_ = 0;
  bool KeepsClusters_ = false;
  double WorkMs_ = 0;
};

[[nodiscard]] std::expected<void, ClusterError>
BuildMeshClusters(RenderGeometryStorage &into, std::span<const Material> surfaces);

}
#endif

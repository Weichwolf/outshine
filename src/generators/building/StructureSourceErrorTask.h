#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTURESOURCEERRORTASK_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTURESOURCEERRORTASK_H
#include "PreparedStructureTile.h"
#include "StructureSurfaceRefinement.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace outshine::Generators {
class StructureSourceErrorTask {
public:
  StructureSourceErrorTask() = default;
  StructureSourceErrorTask(const StructureSourceErrorTask &) = delete;
  StructureSourceErrorTask &operator=(const StructureSourceErrorTask &) = delete;

  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure>
  Reset(const PreparedStructureTile &base,
        const RawTile &view,
        uint64_t sourceKey,
        StructureSurfaceRefinementLimits limits);
  [[nodiscard]] std::expected<StructureSurfaceErrorProgress, StructureSurfaceErrorFailure>
  Step(const StructureMesher &mesher, MeshScratch &scratch, const std::atomic_bool &stopping);
  // Union upper bounds combine by maximum; paired-source lower bounds do not.
  [[nodiscard]] std::optional<StructureSurfaceErrorInterval> Bound(uint64_t sourceKey) const;

private:
  enum class Phase { Source, Reference, Variant, Surface, Complete };
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> SelectSource();
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> BeginMesh();
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> AdvanceMesh(
      const StructureMesher &mesher, MeshScratch &scratch, const std::atomic_bool &stopping);
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> AdvanceSurface();

  const PreparedStructureTile *Base_ = nullptr;
  RawTile View_;
  LevelOfDetail Detail_ = LevelOfDetail::Shell;
  StructureSurfaceRefinementLimits Limits_;
  StructureBakeProgress Progress_;
  std::optional<BakedTile> Reference_, Variant_;
  StructureSurfaceRefinementTask Surface_;
  StructureSurfaceErrorInterval Result_;
  size_t Cursor_ = 0;
  uint32_t Source_ = 0;
  Phase Phase_ = Phase::Complete;
};
}
#endif

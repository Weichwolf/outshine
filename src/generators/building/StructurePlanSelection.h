#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREPLANSELECTION_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREPLANSELECTION_H

#include "StructureBake.h"
#include "StructureMassing.h"

#include <atomic>
#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

namespace outshine::Generators {

class StructurePlanSelection {
public:
  [[nodiscard]] std::optional<double> Add(StructurePlan plan,
                                          StructureMassPlan mass,
                                          size_t footprint,
                                          const StructureMesher &mesher,
                                          MeshScratch &scratch);

  [[nodiscard]] std::expected<void, StructureBakeError> Select(const RawTile &raw,
                                                               const StructureMesher &mesher,
                                                               MeshScratch &scratch,
                                                               BakedTile &out,
                                                               const std::atomic_bool *stopping);

  [[nodiscard]] std::expected<bool, StructureBakeError> Emit(const RawTile &raw,
                                                             const StructureMesher &mesher,
                                                             MeshScratch &scratch,
                                                             BakedTile &out,
                                                             size_t limit,
                                                             const std::atomic_bool *stopping);

private:
  struct Source {
    StructurePlan Plan;
    StructureMassPlan Mass;
    std::optional<Box> Bounds;
    std::optional<double> ShellErrorM;
    size_t CornerFirst = 0, CornerCount = 0, Footprint = 0;
  };

  struct Command {
    size_t Source = 0;
    std::optional<StructureMassPlan> Mass = std::nullopt;
    std::vector<size_t> ProjectedSources;
  };

  [[nodiscard]] std::expected<void, StructureBakeError> EmitProjected(const Command &command,
                                                                      const RawTile &raw,
                                                                      const StructureMesher &mesher,
                                                                      MeshScratch &scratch,
                                                                      BakedTile &out);

  [[nodiscard]] bool SelectProjected(std::span<const size_t> indices,
                                     const RawTile &raw,
                                     const StructureMesher &mesher,
                                     BakedTile &out);

  [[nodiscard]] std::expected<void, StructureBakeError>
  SelectCell(std::span<const size_t> indices,
             const RawTile &raw,
             const StructureMesher &mesher,
             MeshScratch &scratch,
             BakedTile &out,
             const std::atomic_bool *stopping);
  void SelectSource(size_t index,
                    const RawTile &raw,
                    const StructureMesher &mesher,
                    MeshScratch &scratch,
                    BakedTile &out);

  std::vector<Source> Sources_;
  std::vector<double> Corners_;
  std::vector<Command> Commands_;
  size_t Next_ = 0;
};

}
#endif

#ifndef OUTSHINE_GENERATORS_EARTH_TERRAINSHEETTASK_H
#define OUTSHINE_GENERATORS_EARTH_TERRAINSHEETTASK_H

#include "TerrainMesh.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace outshine {
class Tasks;
}

namespace outshine::Generators {
class TerrainInputPreparation;
class TerrainFieldSet;
class TerrainRefinementJob;

class TerrainSheetTask {
public:
  struct Settings {
    TangentFrame Frame;
    TerrainPageLayout Layout;
    int FinestZoom = 0;
    size_t ProbeStride = 0;
  };

  struct Timing {
    double TotalMs = 0;
    double LongestSliceMs = 0;
  };

  struct Completed {
    TerrainSurvey Survey;
    Timing Refinement;
    Timing Halos;
    Timing Surveying;
    double LongestRefinementSourceMs = 0;
    double RefinementDeduplicationMs = 0;
    size_t VirtualSheets = 0;
    size_t Haloed = 0;
  };

  struct Memory {
    size_t Fields = 0;
    size_t Pages = 0;
    size_t Probes = 0;
  };

  using Result = std::expected<Completed, std::string>;

  TerrainSheetTask(Tasks &pool,
                   TerrainInputPreparation &inputs,
                   Patchwork &pages,
                   Settings settings,
                   TerrainRefinementJob refinement);
  ~TerrainSheetTask();
  TerrainSheetTask(const TerrainSheetTask &) = delete;
  TerrainSheetTask &operator=(const TerrainSheetTask &) = delete;

  [[nodiscard]] bool RefinementReady() const noexcept;
  [[nodiscard]] bool HalosReady() const noexcept;
  [[nodiscard]] bool AwaitSlice(double seconds) const;
  [[nodiscard]] std::optional<Result> Take(TerrainInputPreparation &inputs, Patchwork &pages);
  [[nodiscard]] TerrainFieldSet SnapshotSourceFields() const;
  [[nodiscard]] Memory PayloadMemory() const noexcept;

private:
  struct State;
  Tasks &Pool_;
  std::shared_ptr<State> State_;
};
}

#endif

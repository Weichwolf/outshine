#ifndef OUTSHINE_GENERATORS_EARTH_TERRAINSHEETTASK_H
#define OUTSHINE_GENERATORS_EARTH_TERRAINSHEETTASK_H

#include "TerrainMesh.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string_view>

namespace outshine {
class Tasks;
}

namespace outshine::Generators {
class TerrainInputPreparation;

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
    Timing Halos;
    Timing Surveying;
    size_t Haloed = 0;
  };

  struct Memory {
    size_t Fields = 0;
    size_t Pages = 0;
    size_t Probes = 0;
  };

  using Result = std::expected<Completed, std::string_view>;

  TerrainSheetTask(Tasks &pool,
                   TerrainInputPreparation &inputs,
                   Patchwork &pages,
                   Settings settings);
  ~TerrainSheetTask();
  TerrainSheetTask(const TerrainSheetTask &) = delete;
  TerrainSheetTask &operator=(const TerrainSheetTask &) = delete;

  [[nodiscard]] bool HalosReady() const noexcept;
  [[nodiscard]] bool AwaitSlice(double seconds) const;
  [[nodiscard]] std::optional<Result> Take(TerrainInputPreparation &inputs, Patchwork &pages);
  [[nodiscard]] Memory PayloadMemory() const noexcept;

private:
  struct State;
  Tasks &Pool_;
  std::shared_ptr<State> State_;
};
}

#endif

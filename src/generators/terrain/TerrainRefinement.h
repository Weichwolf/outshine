#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINREFINEMENT_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINREFINEMENT_H

#include <array>
#include <cstddef>
#include <expected>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "GroundMesher.h"
#include "TangentFrame.h"
#include "TerrainGrid.h"
#include "TerrainPage.h"
#include "Vec3.h"

namespace outshine::Generators {

struct TerrainRefinementDetail {
  Vec3 EyeM;
  double FocalPx = 0.0;
  double OrthographicPxPerM = 0.0;
  double ErrorPx = 1.0;
};

struct TerrainPatchError {
  double DeviationM = -1.0;
  std::array<float, 2> HeightsM{};
};

using TerrainPatchErrors = std::vector<TerrainPatchError>;

class TerrainRefinementCache {
public:
  explicit TerrainRefinementCache(int side) noexcept : Side_(side) {}

  [[nodiscard]] std::shared_ptr<TerrainPatchErrors>
  Retain(const std::shared_ptr<const outshine::Ground::TerrainField> &field,
         TerrainRefinementCache &previous);

private:
  std::map<std::weak_ptr<const outshine::Ground::TerrainField>,
           std::shared_ptr<TerrainPatchErrors>,
           std::owner_less<>>
      Fields_;
  int Side_;
};

struct TerrainRefinementSource {
  const Sheet *Page = nullptr;
  const outshine::Ground::TerrainField *Heights = nullptr;
  std::shared_ptr<TerrainPatchErrors> Errors;
};

struct TerrainRefinementCorridor {
  EastNorth Start;
  EastNorth End;
  double HalfWidthM = 0.0;
  double MaximumPostingM = 0.0;
};

class TerrainRefinementJob {
public:
  TerrainRefinementJob(std::span<const TerrainRefinementSource> sources,
                       TangentFrame frame,
                       TerrainPageLayout layout,
                       TerrainRefinementDetail detail,
                       size_t maximumPatches,
                       std::span<const TerrainRefinementCorridor> corridors = {});

  [[nodiscard]] std::expected<bool, std::string> Advance(size_t sourcesMost);

  [[nodiscard]] std::vector<Sheet> Take() noexcept;

  [[nodiscard]] double LongestSelectionMs() const noexcept { return LongestSelectionMs_; }

  [[nodiscard]] double LongestSourceMs() const noexcept { return LongestSourceMs_; }

  [[nodiscard]] double DeduplicationMs() const noexcept { return DeduplicationMs_; }

private:
  std::vector<TerrainRefinementSource> Sources_;
  std::vector<TerrainRefinementCorridor> Corridors_;
  TangentFrame Frame_;
  TerrainPageLayout Layout_;
  TerrainRefinementDetail Detail_;
  size_t MaximumPatches_ = 0;
  size_t NextSource_ = 0;
  std::vector<Sheet> Selected_;
  std::vector<Sheet> Result_;
  double LongestSelectionMs_ = 0.0;
  double LongestSourceMs_ = 0.0;
  double DeduplicationMs_ = 0.0;
  bool Complete_ = false;
};

[[nodiscard]] std::expected<std::vector<Sheet>, std::string>
RefineTerrain(std::span<const TerrainRefinementSource> sources,
              const TangentFrame &frame,
              TerrainPageLayout layout,
              TerrainRefinementDetail detail,
              size_t maximumPatches,
              std::span<const TerrainRefinementCorridor> corridors = {});

}
#endif

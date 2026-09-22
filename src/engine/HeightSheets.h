#ifndef OUTSHINE_ENGINE_HEIGHTSHEETS_H
#define OUTSHINE_ENGINE_HEIGHTSHEETS_H

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "GroundMesher.h"
#include "TerrainGrid.h"
#include <optional>
#include <utility>
#include "TerrainLoader.h"
#include <expected>
#include "TangentFrame.h"
#include "TerrainRefinement.h"
#include "TerrainResidency.h"

namespace outshine {

namespace Render {
class SceneRenderer;
}

class HeightSheets {
public:
  struct FieldPreparation {
    int FinestZoom;
    size_t RequestsMost;
  };

  HeightSheets() = default;
  HeightSheets(const HeightSheets &other) = default;
  HeightSheets &operator=(const HeightSheets &) = delete;
  HeightSheets(HeightSheets &&) noexcept = default;
  HeightSheets &operator=(HeightSheets &&) noexcept = default;

  void Into(Render::SceneRenderer *renderer) noexcept { Residency_.Into(renderer); }

  void Framed(const TangentFrame &frame) {
    Frame_ = frame;
    Framed_ = true;
  }

  [[nodiscard]] bool Stitch(Patchwork &laid, std::string &error);
  [[nodiscard]] bool Hands(Patchwork &laid, std::string &error);
  [[nodiscard]] bool BeginResidency(const Patchwork &laid, std::string &error);
  [[nodiscard]] std::expected<bool, std::string> AdvanceResidency(const Patchwork &laid,
                                                                  size_t sheetsMost);
  [[nodiscard]] std::expected<bool, std::string> PrepareFields(const Patchwork &candidate,
                                                               const Ground::GroundStream &ground,
                                                               FieldPreparation preparation);

  [[nodiscard]] bool RefineByError(Patchwork &candidate,
                                   Generators::TerrainPageLayout layout,
                                   Generators::TerrainRefinementDetail detail,
                                   size_t maximumPatches,
                                   std::string &error);
  [[nodiscard]] size_t Halos(Patchwork &laid, int finestZoom);

  [[nodiscard]] std::optional<double> FieldUpM(int zoom, EastNorth at) const;

  [[nodiscard]] std::optional<double> AslMAt(int zoom, LongitudeLatitude at) const;

  [[nodiscard]] const Ground::TerrainField *FieldAt(Data::TileId tile) const;

  void ForgetsFields() {
    Fields_.clear();
    Requests_.clear();
    NextRequest_ = 0;
    ResolvedRequests_ = 0;
    RequestsPrepared_ = false;
  }

  void Clear();

  [[nodiscard]] size_t Standing() const { return Residency_.Standing(); }

  [[nodiscard]] size_t Instances() const { return Residency_.Instances(); }

  [[nodiscard]] size_t Flat() const { return Residency_.Flat(); }

  [[nodiscard]] uint64_t Digest() const;

  [[nodiscard]] size_t RimsMissing() const { return RimsMissing_; }

  [[nodiscard]] size_t HeapBytes() const noexcept;

  struct SeamKind {
    double EvenM = 0.0;
    double OddBeforeM = 0.0;
    double OddAfterM = 0.0;
    double WorstLongitudeDeg = 0.0;
    double WorstLatitudeDeg = 0.0;
    int WorstFineZoom = 0;
    int WorstCoarseZoom = 0;
    size_t Edges = 0;
  };

  struct Seam {
    SeamKind Virtual;
    SeamKind Real;
  };

  [[nodiscard]] const Seam &Seams() const { return Seams_; }

private:
  [[nodiscard]] bool StitchEdges(Patchwork &laid, std::string &error);
  [[nodiscard]] std::optional<float> AslAt(int zoom, Ground::TileFrac at) const;
  [[nodiscard]] bool HaloOf(Sheet &sheet, int finestZoom);

  struct FieldRequest {
    Data::TileId Tile;
    bool Resolved = false;
  };

  std::vector<std::pair<Data::TileId, std::shared_ptr<const Ground::TerrainField>>> Fields_;
  std::vector<FieldRequest> Requests_;
  size_t NextRequest_ = 0;
  size_t ResolvedRequests_ = 0;
  bool RequestsPrepared_ = false;
  size_t RimsMissing_ = 0;
  Seam Seams_;
  TerrainResidency Residency_;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};

}
#endif

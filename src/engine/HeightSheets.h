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

  [[nodiscard]] bool RefineByError(Patchwork &candidate,
                                   const Ground::GroundStream &ground,
                                   Generators::TerrainPageLayout layout,
                                   Generators::TerrainRefinementDetail detail,
                                   size_t maximumPatches,
                                   std::string &error);
  [[nodiscard]] size_t Halos(Patchwork &laid, const Ground::GroundStream &ground, int finestZoom);

  [[nodiscard]] std::optional<double> FieldUpM(int zoom, EastNorth at) const;

  [[nodiscard]] std::optional<double> AslMAt(int zoom, LongitudeLatitude at) const;

  void ForgetsFields() { Fields_.clear(); }

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
    size_t Edges = 0;
  };

  struct Seam {
    SeamKind Virtual;
    SeamKind Real;
  };

  [[nodiscard]] const Seam &Seams() const { return Seams_; }

private:
  [[nodiscard]] bool StitchEdges(Patchwork &laid, std::string &error);
  [[nodiscard]] const Ground::TerrainField *FieldAt(const Ground::GroundStream &ground,
                                                    Data::TileId tile);
  [[nodiscard]] const Ground::TerrainField *HeldFieldAt(Data::TileId tile) const;
  static void AsksFields(const Ground::GroundStream &ground, const Patchwork &laid, int finestZoom);
  [[nodiscard]] std::optional<float>
  AslAt(const Ground::GroundStream &ground, int zoom, Ground::TileFrac at);
  [[nodiscard]] bool HaloOf(Sheet &sheet, const Ground::GroundStream &ground, int finestZoom);

  std::vector<std::pair<Data::TileId, std::shared_ptr<const Ground::TerrainField>>> Fields_;
  size_t RimsMissing_ = 0;
  Seam Seams_;
  TerrainResidency Residency_;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};

}
#endif

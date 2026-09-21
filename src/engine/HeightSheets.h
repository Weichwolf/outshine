#ifndef OUTSHINE_ENGINE_HEIGHTSHEETS_H
#define OUTSHINE_ENGINE_HEIGHTSHEETS_H

#include <cstddef>
#include <memory>
#include <map>
#include <tuple>
#include <span>
#include <string>
#include <vector>

#include "Address.h"
#include "GroundMesher.h"
#include "TerrainGrid.h"
#include <optional>
#include <utility>
#include "TerrainLoader.h"
#include "scene/TerrainTile.h"
#include <expected>
#include "TangentFrame.h"
#include "TerrainRefinement.h"

namespace outshine {

namespace Render {
class SceneRenderer;
}

class HeightSheets {
public:
  void Into(Render::SceneRenderer *renderer) noexcept { Renderer_ = renderer; }

  void Framed(const TangentFrame &frame) {
    Frame_ = frame;
    Framed_ = true;
  }

  [[nodiscard]] bool Hands(Patchwork &laid, std::string &error);

  struct Nearer {
    int FinestZoom = 0;
    int Levels = 0;
    LongitudeLatitude Eye;
  };

  [[nodiscard]] static size_t Refine(Patchwork &laid, Nearer how);

  [[nodiscard]] bool RefineByError(Patchwork &candidate,
                                   const Ground::GroundStream &ground,
                                   Generators::TerrainPageLayout layout,
                                   Generators::TerrainRefinementDetail detail,
                                   size_t maximumPatches,
                                   std::string &error);
  [[nodiscard]] size_t Halos(Patchwork &laid, const Ground::GroundStream &ground, int finestZoom);

  [[nodiscard]] std::optional<double> FieldUpM(int zoom, EastNorth at) const;

  void ForgetsFields() { Fields_.clear(); }

  void Clear();

  [[nodiscard]] size_t Standing() const { return Held_.size(); }

  [[nodiscard]] size_t Instances() const { return Instances_.size() + Virtual_.size(); }

  [[nodiscard]] size_t Flat() const { return Flat_; }

  [[nodiscard]] uint64_t Digest() const;

  [[nodiscard]] size_t RimsMissing() const { return RimsMissing_; }

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
  struct Held {
    Data::TileId Tile;
    Render::HeightPageHandle Page{};
    std::vector<float> Nodes;
  };

  [[nodiscard]] bool HandsGrid(const Patchwork &laid, std::string &error);
  void StitchEdges(Patchwork &laid);
  [[nodiscard]] std::expected<Render::HeightPageHandle, std::string>
  PageFor(Data::TileId tile, std::span<const float> nodes);
  [[nodiscard]] Render::TerrainTile
  TileOf(Data::TileId tile, Render::HeightPageHandle page, std::span<const float> nodes) const;

  std::vector<Held> Held_;
  std::map<std::tuple<int, uint32_t, uint32_t>, size_t> PageIndex_;
  std::vector<Render::TerrainTile> Instances_;
  std::vector<Render::TerrainTile> Virtual_;
  [[nodiscard]] const Ground::TerrainField *FieldAt(const Ground::GroundStream &ground,
                                                    Data::TileId tile);
  [[nodiscard]] const Ground::TerrainField *HeldFieldAt(Data::TileId tile) const;
  static void AsksFields(const Ground::GroundStream &ground, const Patchwork &laid, int finestZoom);
  [[nodiscard]] std::optional<float>
  AslAt(const Ground::GroundStream &ground, int zoom, Ground::TileFrac at);
  [[nodiscard]] bool HaloOf(Sheet &sheet, const Ground::GroundStream &ground, int finestZoom);

  std::vector<std::pair<Data::TileId, std::shared_ptr<const Ground::TerrainField>>> Fields_;
  size_t Flat_ = 0;
  size_t RimsMissing_ = 0;
  Seam Seams_;
  uint32_t GridPostings_ = 0;
  Render::SceneRenderer *Renderer_ = nullptr;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};

}
#endif

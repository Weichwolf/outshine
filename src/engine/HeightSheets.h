#ifndef OUTSHINE_ENGINE_HEIGHTSHEETS_H
#define OUTSHINE_ENGINE_HEIGHTSHEETS_H

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "Address.h"
#include "GroundMesher.h"
#include "GroundYield.h"
#include "TerrainGrid.h"
#include <optional>
#include <utility>
#include "TerrainLoader.h"
#include "SubjectTypes.h"
#include "TangentFrame.h"

namespace outshine {

namespace Core {
class Live;
}

class HeightSheets {
public:
  void Into(Core::Live *live) { Live_ = live; }

  void Framed(const TangentFrame &frame) {
    Frame_ = frame;
    Framed_ = true;
  }

  struct Soup {
    std::vector<float> PositionM;
    std::vector<uint32_t> Index;
    double TallestM = 0.0;
    double LowestM = 0.0;
    double TallestOutM = 0.0;
  };

  [[nodiscard]] Soup SoupOf(const Patchwork &laid, int zoomAtLeast = 0) const;

  [[nodiscard]] bool Hands(const Patchwork &laid, std::string &error);

  struct Pressed {
    size_t Nodes = 0;
    size_t Structures = 0;
    size_t Held = 0;
    double DeepestM = 0.0;
    double RaisedM = 0.0;
    Floors Pads;
    Floors Corridors;
  };

  [[nodiscard]] Pressed
  Press(std::span<const Yields> yields, Patchwork &laid, double mostEarthworkM) const;

  struct Nearer {
    int FinestZoom = 0;
    int Levels = 0;
    LongitudeLatitude Eye;
  };

  [[nodiscard]] static size_t Refine(Patchwork &laid, Nearer how);
  [[nodiscard]] size_t Halos(Patchwork &laid, const Ground::GroundStream &ground, int finestZoom);

  [[nodiscard]] std::optional<double>
  FieldUpM(const Ground::GroundStream &ground, int zoom, EastNorth at);

  void ForgetsFields() { Fields_.clear(); }

  void Clear();

  [[nodiscard]] size_t Standing() const { return Held_.size(); }

  [[nodiscard]] size_t Instances() const { return Instances_.size() + Virtual_.size(); }

  [[nodiscard]] size_t Flat() const { return Flat_; }

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
    Render::PageId Page = Render::kNoPage;
    bool Wanted = false;
    std::vector<float> Nodes;
  };

  [[nodiscard]] bool HandsGrid(const Patchwork &laid, std::string &error);
  void MeasuresSeams(const Sheet &fine, const Patchwork &laid, std::array<float, 4> &stitched);
  [[nodiscard]] std::array<float, 4> StitchOf(const Sheet &sheet,
                                              const Patchwork &laid,
                                              std::span<const Data::TileId> present,
                                              int coarsest);
  [[nodiscard]] Render::PageId
  PageFor(Data::TileId tile, std::span<const float> nodes, std::string &error);
  [[nodiscard]] Render::GroundTile TileOf(Data::TileId tile,
                                          Render::PageId page,
                                          std::span<const float> nodes,
                                          std::array<float, 4> stitched) const;

  std::vector<Held> Held_;
  std::vector<Render::GroundTile> Instances_;
  std::vector<Render::GroundTile> Virtual_;
  [[nodiscard]] const Ground::TerrainField *FieldAt(const Ground::GroundStream &ground,
                                                    Data::TileId tile);
  static void AsksFields(const Ground::GroundStream &ground, const Patchwork &laid, int finestZoom);
  [[nodiscard]] std::optional<float>
  AslAt(const Ground::GroundStream &ground, int zoom, Ground::TileFrac at);
  [[nodiscard]] bool HaloOf(Sheet &sheet, const Ground::GroundStream &ground, int finestZoom);

  std::vector<std::pair<Data::TileId, std::shared_ptr<const Ground::TerrainField>>> Fields_;
  size_t Flat_ = 0;
  size_t RimsMissing_ = 0;
  Seam Seams_;
  uint32_t GridPostings_ = 0;
  Core::Live *Live_ = nullptr;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};

} // namespace outshine
#endif

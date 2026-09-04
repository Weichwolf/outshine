#ifndef OUTSHINE_ENGINE_HEIGHTSHEETS_H
#define OUTSHINE_ENGINE_HEIGHTSHEETS_H

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "Address.h"
#include "GroundMesher.h"
#include "GroundYield.h"
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

  [[nodiscard]] bool Hands(const Patchwork &laid, std::string &error);
  [[nodiscard]] size_t Press(std::span<const Yields> yields, Patchwork &laid) const;

  struct Nearer {
    int FinestZoom = 0;
    int Levels = 0;
    LongitudeLatitude Eye;
  };

  [[nodiscard]] static size_t
  Refine(Patchwork &laid, const Ground::GroundStream &ground, Nearer how);
  void Clear();

  [[nodiscard]] size_t Standing() const { return Held_.size(); }

  [[nodiscard]] size_t Instances() const { return Instances_.size() + Virtual_.size(); }

private:
  struct Held {
    Data::TileId Tile;
    Render::PageId Page = Render::kNoPage;
    bool Wanted = false;
  };

  [[nodiscard]] bool HandsGrid(const Patchwork &laid, std::string &error);
  [[nodiscard]] Render::PageId
  PageFor(Data::TileId tile, std::span<const float> nodes, std::string &error);
  [[nodiscard]] Render::GroundInstance InstanceOf(Data::TileId tile, Render::PageId page) const;

  std::vector<Held> Held_;
  std::vector<Render::GroundInstance> Instances_;
  std::vector<Render::GroundInstance> Virtual_;
  Render::PageId Zero_ = Render::kNoPage;
  uint32_t GridPostings_ = 0;
  Core::Live *Live_ = nullptr;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};

} // namespace outshine
#endif

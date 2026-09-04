#ifndef OUTSHINE_ENGINE_HEIGHTSHEETS_H
#define OUTSHINE_ENGINE_HEIGHTSHEETS_H

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "Address.h"
#include "GroundMesher.h"
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
  void Clear();

  [[nodiscard]] size_t Standing() const { return Held_.size(); }

  [[nodiscard]] size_t Instances() const { return Instances_.size(); }

private:
  struct Held {
    Data::TileId Tile;
    Render::PageId Page = Render::kNoPage;
    bool Wanted = false;
  };

  [[nodiscard]] Render::PageId
  PageFor(Data::TileId tile, std::span<const float> nodes, std::string &error);
  [[nodiscard]] Render::GroundInstance InstanceOf(Data::TileId tile, Render::PageId page) const;

  std::vector<Held> Held_;
  std::vector<Render::GroundInstance> Instances_;
  Render::PageId Zero_ = Render::kNoPage;
  Core::Live *Live_ = nullptr;
  TangentFrame Frame_ = TangentFrame::At({});
  bool Framed_ = false;
};

} // namespace outshine
#endif

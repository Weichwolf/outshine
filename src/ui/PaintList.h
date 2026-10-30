#ifndef OUTSHINE_UI_PAINTLIST_H
#define OUTSHINE_UI_PAINTLIST_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Layout.h"

namespace outshine::Ui {

struct Quad {
  double X = 0, Y = 0, Width = 0, Height = 0;
  double U0 = 0, V0 = 0, U1 = 0, V1 = 0;
  uint32_t Colour = 0;
  double Radius = 0;
  double Opacity = 1.0;

  double ClipX = 0, ClipY = 0, ClipWidth = 0, ClipHeight = 0;

  int Node = -1;
};

inline constexpr size_t kQuadBound = 16384;

class PaintList {
public:
  [[nodiscard]] bool Build(const Layout &layout, const Font &font, std::string &error);

  [[nodiscard]] const std::vector<Quad> &Quads() const { return Quads_; }

private:
  std::vector<Quad> Quads_;
};

}
#endif

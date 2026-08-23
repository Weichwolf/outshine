#ifndef UI_PAINT_H
#define UI_PAINT_H

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

struct Page {
  double OffsetY = 0;
  double HeightPx = 0;
};

[[nodiscard]] std::vector<double> PageBreaks(const Layout &layout, double pageHeightPx,
                                             size_t &linesTallerThanThePage);

class Painting {
public:

  [[nodiscard]] bool Build(const Layout &layout, const Font &font, std::string &error,
                           const Page &page = {});

  [[nodiscard]] const std::vector<Quad> &Quads() const { return Quads_; }

  [[nodiscard]] size_t QuadsBeyondTheBound() const { return Beyond_; }

private:
  std::vector<Quad> Quads_;
  size_t Beyond_ = 0;
};

}
#endif

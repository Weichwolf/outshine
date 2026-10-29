#ifndef OUTSHINE_UI_LAYOUT_H
#define OUTSHINE_UI_LAYOUT_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Font.h"
#include "Markup.h"
#include "Style.h"

namespace outshine::Ui {

struct Edges {
  double Top = 0, Right = 0, Bottom = 0, Left = 0;
};

struct Area {
  double X = 0, Y = 0, Width = 0, Height = 0;
};

struct Box {
  int Node = -1;
  double X = 0, Y = 0, Width = 0, Height = 0;
  Edges Margin, Border, Padding;
  uint32_t Background = 0, BorderColour = 0;
  double Radius = 0, Opacity = 1.0;
  bool Clips = false;
  bool Scrolls = false;
  double ScrolledPx = 0.0;
  double ContentPx = 0.0;

  bool Positioned = false;

  [[nodiscard]] double Top() const { return Border.Top; }

  int Parent = -1;
  std::vector<int> Children;

  double Baseline = 0;

  std::string Text;
  double FontSize = 0;
  Family Face = Family{};
  uint32_t Colour = 0;
};

class Layout {
public:
  struct Scrolled {
    int Node = -1;
    double Px = 0.0;
  };

  struct Under {
    size_t Box = 0;
  };

  [[nodiscard]] bool DescendsFrom(size_t at, Under root) const;
  [[nodiscard]] double ReachesUnder(Under root, double from) const;
  void ShiftUnder(Under root, double byPx);
  void SettleScrollers(std::span<const Scrolled> scrolled);

  [[nodiscard]] bool Build(const Markup &markup,
                           Stylesheet &sheet,
                           double viewportWidth,
                           double viewportHeight,
                           const Font &font,
                           std::string &error) {
    return Build(markup, sheet, viewportWidth, viewportHeight, font, {}, error);
  }

  [[nodiscard]] bool Build(const Markup &markup,
                           Stylesheet &sheet,
                           double viewportWidth,
                           double viewportHeight,
                           const Font &font,
                           std::span<const Scrolled> scrolled,
                           std::string &error);

  [[nodiscard]] int Scroller(double x, double y) const;
  [[nodiscard]] double ScrollableBy(int node) const;

  [[nodiscard]] const std::vector<Box> &Boxes() const { return Boxes_; }

  [[nodiscard]] int Hit(double x, double y) const;

  struct Work {
    size_t Places = 0;
    size_t Measures = 0;
    size_t MeasureHits = 0;
    size_t Baselines = 0;
    size_t BaselineHits = 0;
    size_t Intrinsics = 0;
    size_t IntrinsicHits = 0;
  };

  [[nodiscard]] const Work &Spent() const { return Spent_; }

  [[nodiscard]] double ViewportWidth() const { return ViewportWidth_; }

  [[nodiscard]] double ViewportHeight() const { return ViewportHeight_; }

private:
  std::vector<Box> Boxes_;
  Work Spent_;
  double ViewportWidth_ = 0, ViewportHeight_ = 0;
};

[[nodiscard]] const char *UserAgentSheet();

[[nodiscard]] bool ElementIsInTheSubset(std::string_view tag);

[[nodiscard]] std::vector<std::string> ElementsOutsideTheSubset(const Markup &markup);

}
#endif

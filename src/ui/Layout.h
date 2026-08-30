#ifndef OUTSHINE_UI_LAYOUT_H
#define OUTSHINE_UI_LAYOUT_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Markup.h"
#include "Style.h"

namespace outshine::Ui {

struct FontMetrics {
  double Advance = 0;
  double Ascent = 0;
  double Descent = 0;
};

struct Glyph {
  double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  double U0 = 0, V0 = 0, U1 = 0, V1 = 0;

  double AdvancePx = 0;
  bool Drawn = false;
};

enum class Family : uint8_t;

[[nodiscard]] Family FamilyOf(uint32_t declared);

struct Font {
  virtual ~Font() = default;
  [[nodiscard]] virtual FontMetrics At(double sizePx, Family family) const = 0;

  [[nodiscard]] virtual Glyph Shape(char32_t code, double sizePx, Family family) const {
    (void)code;
    (void)sizePx;
    (void)family;
    return {};
  }

  [[nodiscard]] virtual const uint8_t *Sheet(void) const { return nullptr; }

  [[nodiscard]] virtual int SheetWidthPx(void) const { return 0; }

  [[nodiscard]] virtual int SheetHeightPx(void) const { return 0; }

  [[nodiscard]] virtual uint64_t Cut(void) const { return 0; }
};

struct AhemFont final : Font {
  [[nodiscard]] FontMetrics At(double sizePx, Family family) const override {
    (void)family;
    return {sizePx, sizePx * 0.8, sizePx * 0.2};
  }

  [[nodiscard]] Glyph Shape(char32_t code, double sizePx, Family family) const override {
    (void)family;
    if (code == U' ') { return {0, 0, 0, 0, 0, 0, 0, 0, sizePx, false}; }
    return {0.0, 0.0, sizePx, sizePx, 0, 0, 0, 0, sizePx, true};
  }
};

struct Edges {
  double Top = 0, Right = 0, Bottom = 0, Left = 0;
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

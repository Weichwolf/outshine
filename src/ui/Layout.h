/* WHERE EVERY BOX LANDS, IN THE VIEWPORT'S OWN PIXELS.
 *
 * **This is the measure-and-place half of the engine** (board:1442): markup and style go in, boxes come
 * out, and nothing here knows a device, a texture or a draw call. That is what makes a layout judgeable
 * with no GPU in scope -- and it is the whole reason the corpus's `data-expected-width` family can hold
 * this engine to account the day it exists, long before anything is painted.
 *
 * **THE USER-AGENT SHEET IS PART OF THE DECLARATION AND NOT A DETAIL.** [MEASURED] a WPT layout case
 * states `data-offset-x="8"`, which IS `body`'s own margin, so an engine without the sheet is wrong by
 * eight pixels on every such case and looks like it has a layout defect. What is in it is written down
 * in `UserAgentSheet()` and nowhere else.
 *
 * **A LENGTH IS RESOLVED WHERE ITS CONTAINER IS KNOWN AND NEVER BEFORE.** A percentage that met its
 * container early is the same defect as a number that lost its unit, one sentence shorter. */
#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <cstdint>
#include <string>
#include <vector>

#include "Markup.h"
#include "Style.h"

namespace outshine::Ui {

/* THE METRICS A FONT ANSWERS WITH, and the layout asks for nothing else. `Ahem` -- the font WPT uses so
 * that a layout test is not a font test -- answers `Advance = Size` and `Ascent = 0.8 * Size`, which is
 * what makes a measured line exact rather than approximately right. */
struct FontMetrics {
  double Advance = 0;   /* one glyph's width at the given size */
  double Ascent = 0;
  double Descent = 0;
};

/* A CONSUMER'S FONT, ASKED PER SIZE. The engine measures with it and never opens a file: who makes an
 * asset is not the engine's business, and a font is an asset. */
struct Font {
  virtual ~Font() = default;
  [[nodiscard]] virtual FontMetrics At(double sizePx) const = 0;
};

/* THE FONT THE CORPUS MEASURES WITH: every glyph a solid square of the em, ascent four fifths of it. */
struct AhemFont final : Font {
  [[nodiscard]] FontMetrics At(double sizePx) const override {
    return {sizePx, sizePx * 0.8, sizePx * 0.2};
  }
};

struct Edges {
  double Top = 0, Right = 0, Bottom = 0, Left = 0;
};

/* ONE BOX AS THE LAYOUT LEAVES IT. The rectangle is the BORDER box in the viewport's pixels, which is
 * the one a painter needs and the one a layout assertion states. */
struct Box {
  int Node = -1;
  double X = 0, Y = 0, Width = 0, Height = 0;
  Edges Margin, Border, Padding;
  uint32_t Background = 0, BorderColour = 0;
  double Radius = 0, Opacity = 1.0;
  bool Clips = false;
  int Parent = -1;
  std::vector<int> Children;
  /* A run of text and the size it was measured at, empty on every other box. */
  std::string Text;
  double FontSize = 0;
  uint32_t Colour = 0;
};

class Layout {
public:
  /* `viewport` is the surface the declaration is laid out for -- a HUD's frame, a screen's texture or a
   * book page, and the engine cannot tell which. */
  /* `sheet` is not `const`: an inline style is READ here, and what it declares outside the subset is
   * counted into the sheet's own tallies rather than dropped (board:1445). */
  [[nodiscard]] bool Build(const Markup &markup, Stylesheet &sheet, double viewportWidth,
                           double viewportHeight, const Font &font, std::string &error);

  [[nodiscard]] const std::vector<Box> &Boxes(void) const { return Boxes_; }
  /* Which box a point falls in, deepest first, or -1. **The library answers what was hit and decides
   * nothing about it** -- what the hit MEANS is the client's, which is why this returns a node and not
   * an action. */
  [[nodiscard]] int Hit(double x, double y) const;

private:
  std::vector<Box> Boxes_;
};

/* WHAT AN ELEMENT MEANS BEFORE ANY SHEET SPEAKS, and the list is the whole list. */
[[nodiscard]] const char *UserAgentSheet(void);

/* THE OTHER HALF OF THE DECLARED SUBSET, AND IT IS ABOUT ELEMENTS RATHER THAN PROPERTIES. A sheet's
 * properties say what this engine can express; they say nothing about which ELEMENTS it models, and a
 * declaration is only inside the subset when both are. [MEASURED] the first corpus run let ten cases
 * through on the property count alone and six of them were about `<img>`, `<input>` and `<embed>` --
 * boxes whose size comes from a resource this engine never loaded -- so the number named a population
 * wider than the claim it decided.
 *
 * IT IS AN ALLOWLIST BECAUSE A BLOCKLIST CANNOT BE FINISHED. An element nobody has thought of is laid
 * out as an ordinary box under a blocklist and reported as held; under this list it is outside, which
 * is the answer that stays true when upstream grows a tag. */
[[nodiscard]] bool ElementIsInTheSubset(std::string_view tag);
/* Every element of this markup that is not, each named once, in the document's own order. */
[[nodiscard]] std::vector<std::string> ElementsOutsideTheSubset(const Markup &markup);

} // namespace outshine::Ui
#endif

/* THE ENGINE SETS PROSE IN A FONT IT DID NOT WRITE (board:1442).
 *
 * **AHEM IS THE MEASUREMENT FACE AND IT IS MONOSPACE**, so every claim proved with it is also true of
 * an engine that divides a width by one advance and calls it a measurement. This file exists to make
 * that impossible: the face here has a DIFFERENT advance per glyph and a patch of an atlas for each,
 * which is what a real face is, and the numbers below are what the run must come out to.
 *
 * **THE FONT IS THE CONSUMER'S AND THE ENGINE NEVER OPENS A FILE.** Who makes an asset is not the
 * engine's business; what the engine owes is an interface a real face fits through, and this is the
 * test of that claim rather than of any particular face. */
#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"
#include "Layout.h"
#include "Paint.h"

using namespace outshine::Test;
using namespace outshine::Ui;

namespace {

/* A PROPORTIONAL FACE, ARITHMETIC ENOUGH TO BE CHECKED BY HAND: `i` is a quarter of the em, `m` is a
 * whole one, everything else is half, and a space is a third. The atlas is a sixteen-column grid, so
 * every glyph's patch is a different rectangle and a painter that ignored them would still place
 * boxes and would place the wrong picture in them. */
struct GridFont final : Font {
  [[nodiscard]] FontMetrics At(double sizePx) const override {
    return {sizePx * 0.5, sizePx * 0.8, sizePx * 0.2};
  }
  [[nodiscard]] Glyph Shape(char32_t code, double sizePx) const override {
    const double advance = code == U'i'   ? sizePx * 0.25
                           : code == U'm' ? sizePx
                           : code == U' ' ? sizePx / 3.0
                                          : sizePx * 0.5;
    if (code == U' ') { return {0, 0, 0, 0, 0, 0, 0, 0, advance, false}; }
    const double column = (double)(code % 16u);
    return {0.0, 0.0, advance, sizePx * 0.8, column / 16.0, 0.0, (column + 1) / 16.0, 1.0, advance,
            true};
  }
};

struct Built {
  Markup Tree;
  Stylesheet Sheet;
  Layout Placed;
  Painting Painted;
};

bool Set(Built &built, const char *markup, double width, double height) {
  std::string error;
  if (!built.Tree.Read(markup, error)) {
    std::printf("       the document was refused: %s\n", error.c_str());
    return false;
  }
  built.Sheet.Read(UserAgentSheet());
  built.Sheet.Read(built.Tree.StyleText());
  const GridFont font;
  if (!built.Placed.Build(built.Tree, built.Sheet, width, height, font, error)) {
    std::printf("       the layout was refused: %s\n", error.c_str());
    return false;
  }
  if (!built.Painted.Build(built.Placed, font, error)) {
    std::printf("       the painting was refused: %s\n", error.c_str());
    return false;
  }
  return true;
}

int Lines(const Built &built) {
  int count = 0;
  for (const Box &box : built.Placed.Boxes()) {
    if (!box.Text.empty()) { ++count; }
  }
  return count;
}

}  // namespace

int main(void) {
  /* A RUN IS AS WIDE AS ITS OWN GLYPHS AND NOT AS WIDE AS ITS CHARACTER COUNT. `mmmm` at 20 px is
   * four ems -- 80 -- where a monospace reading of the same face would answer four half-ems, 40. */
  {
    Built built;
    if (Set(built, "<style>body{margin:0}p{font-size:20px;line-height:1;margin:0;width:400px;"
                   "color:#000000}</style><body><p>mmmm</p></body>",
            500, 200)) {
      const Box *run = nullptr;
      for (const Box &box : built.Placed.Boxes()) {
        if (!box.Text.empty()) { run = &box; }
      }
      CHECK(run != nullptr, "the run is laid out");
      if (run != nullptr) {
        CHECK(std::fabs(run->Width - 80.0) < 1e-9,
              "four ems of the widest glyph measure 80 px, where one advance for every character "
              "would have answered 40 -- the number that separates a measurement from an assumption");
      }
    }
  }

  /* THE PEN MOVES BY EACH GLYPH'S OWN ADVANCE. `im` is a quarter then a whole, so the second glyph
   * begins at 5 and is 20 wide -- a painter walking a fixed advance would put it at 10. */
  {
    Built built;
    if (Set(built, "<style>body{margin:0}p{font-size:20px;line-height:1;margin:0;width:400px;"
                   "color:#000000}</style><body><p>im</p></body>",
            500, 200)) {
      CHECK(built.Painted.Quads().size() == 2, "two glyphs, two rectangles");
      if (built.Painted.Quads().size() == 2) {
        const Quad &first = built.Painted.Quads()[0];
        const Quad &second = built.Painted.Quads()[1];
        CHECK(first.X == 0 && std::fabs(first.Width - 5.0) < 1e-9,
              "the narrow glyph is a quarter of the em");
        CHECK(std::fabs(second.X - 5.0) < 1e-9,
              "and the next begins where the first ENDED, not one fixed advance along");
        CHECK(std::fabs(second.Width - 20.0) < 1e-9, "at its own width of a whole em");
        CHECK(first.U0 != second.U0 && first.U1 > first.U0,
              "each carries its own patch of the consumer's atlas, which is what a face the engine "
              "did not write actually needs from this interface");
      }
    }
  }

  /* THE WRAP HAPPENS WHERE THE ROOM RUNS OUT, WHICH A CHARACTER COUNT CANNOT KNOW. Six `m` at 10 px
   * is 60; in 45 px of room four fit on the first line and two follow, and the break is at the space
   * because no word is ever cut in this subset. */
  {
    Built built;
    if (Set(built, "<style>body{margin:0}p{font-size:10px;line-height:1;margin:0;width:45px;"
                   "color:#000000}</style><body><p>mm mm mm</p></body>",
            200, 200)) {
      CHECK(Lines(built) == 2,
            "the run breaks into two lines, and it breaks on the width its own glyphs took");
    }
  }

  /* A WORD WIDER THAN THE LINE IS PLACED AND OVERFLOWS. No hyphenation and no break inside a word is
   * a DECLARED part of this subset, so the answer is one line that reaches past its box rather than a
   * word cut in half or a loop that cannot advance. */
  {
    Built built;
    if (Set(built, "<style>body{margin:0}p{font-size:10px;line-height:1;margin:0;width:12px;"
                   "color:#000000}</style><body><p>mmmm</p></body>",
            200, 200)) {
      CHECK(Lines(built) == 1, "the word that cannot fit stays whole on one line");
    }
  }

  return Report();
}

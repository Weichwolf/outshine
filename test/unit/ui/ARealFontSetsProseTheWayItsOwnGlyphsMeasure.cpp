#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"
#include "Layout.h"
#include "Paint.h"

using namespace outshine::Test;
using namespace outshine::Ui;

namespace {

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

}

int main(void) {

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

  {
    Built built;
    if (Set(built, "<style>body{margin:0}p{font-size:10px;line-height:1;margin:0;width:45px;"
                   "color:#000000}</style><body><p>mm mm mm</p></body>",
            200, 200)) {
      CHECK(Lines(built) == 2,
            "the run breaks into two lines, and it breaks on the width its own glyphs took");
    }
  }

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

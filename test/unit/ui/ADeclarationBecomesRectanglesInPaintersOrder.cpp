#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Layout.h"
#include "Paint.h"

using namespace outshine::Test;
using namespace outshine::Ui;

namespace {

struct Built {
  Markup Tree;
  Stylesheet Sheet;
  Layout Placed;
  Painting Painted;
};

bool Paint(Built &built, const char *markup, double width, double height) {
  std::string error;
  if (!built.Tree.Read(markup, error)) {
    std::printf("       the document was refused: %s\n", error.c_str());
    return false;
  }
  built.Sheet.Read(UserAgentSheet());
  built.Sheet.Read(built.Tree.StyleText());
  const AhemFont font;
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

int At(const Painting &painting, double x, double y, double w, double h) {
  for (size_t i = 0; i < painting.Quads().size(); ++i) {
    const Quad &q = painting.Quads()[i];
    if (q.X == x && q.Y == y && q.Width == w && q.Height == h) { return (int)i; }
  }
  return -1;
}

}

int main(void) {

  {
    Built built;
    if (Paint(built, "<style>#a{width:100px;height:50px;background:#ff0000}</style>"
                     "<body><div id=a></div></body>",
              300, 200)) {
      const int box = At(built.Painted, 8, 8, 100, 50);
      CHECK(box >= 0, "a declared box is a rectangle where the layout put it");
      if (box >= 0) {
        CHECK(built.Painted.Quads()[(size_t)box].Colour == 0xFF0000FFu,
              "and it carries the colour the declaration named");
      }
      CHECK(built.Painted.Quads().size() == 1,
            "a document of one coloured box paints exactly one rectangle, because a background "
            "nobody declared reaches no pixel and costs no quad");
    }
  }

  {
    Built built;
    if (Paint(built, "<style>#a{width:100px;height:40px;background:#00ff00;"
                     "border-width:2px 4px 6px 8px;border-color:#0000ff;box-sizing:border-box}"
                     "</style><body><div id=a></div></body>",
              300, 200)) {
      CHECK(At(built.Painted, 8, 8, 100, 2) >= 0, "the top edge is the declared 2 px");
      CHECK(At(built.Painted, 8, 42, 100, 6) >= 0, "the bottom edge is the declared 6 px");
      CHECK(At(built.Painted, 8, 10, 8, 32) >= 0, "the left edge is 8 px and sits between the two");
      CHECK(At(built.Painted, 104, 10, 4, 32) >= 0, "the right edge is 4 px at the box's far side");
      CHECK(built.Painted.Quads().size() == 5,
            "a background and four edges, and the background is FIRST -- painter's order is the "
            "whole of the depth question in this subset");
      CHECK(built.Painted.Quads()[0].Colour == 0x00FF00FFu,
            "the background covers the border box and the border is painted over it, which is CSS's "
            "own default rather than a simplification");
    }
  }

  {
    Built built;
    if (Paint(built, "<style>#o{width:50px;height:50px;overflow:hidden}"
                     "#i{width:200px;height:200px;background:#123456}</style>"
                     "<body><div id=o><div id=i></div></div></body>",
              300, 300)) {
      const int inner = At(built.Painted, 8, 8, 200, 200);
      CHECK(inner >= 0, "the inner box keeps the size it was given and is not resized by the clip");
      if (inner >= 0) {
        const Quad &q = built.Painted.Quads()[(size_t)inner];
        CHECK(q.ClipX == 8 && q.ClipY == 8 && q.ClipWidth == 50 && q.ClipHeight == 50,
              "and it carries the clipping ancestor's rectangle, not its own");
      }
    }
  }

  {
    Built built;
    if (Paint(built, "<style>#o{opacity:0.5}#i{width:10px;height:10px;background:#ffffff;"
                     "opacity:0.5}</style><body><div id=o><div id=i></div></div></body>",
              300, 300)) {
      const int inner = At(built.Painted, 8, 8, 10, 10);
      CHECK(inner >= 0, "the nested box is painted");
      if (inner >= 0) {
        CHECK(std::fabs(built.Painted.Quads()[(size_t)inner].Opacity - 0.25) < 1e-12,
              "and its opacity is the product of both declarations, not the nearer one");
      }
    }
  }

  {
    Built built;
    if (Paint(built, "<style>p{font-size:10px;line-height:1;margin:0;color:#000000}</style>"
                     "<body><p>ab cd</p></body>",
              300, 300)) {
      CHECK(built.Painted.Quads().size() == 4,
            "five characters and one space paint four glyphs, because a space covers nothing and an "
            "empty quad is a rectangle nobody asked for");
      CHECK(At(built.Painted, 8, 8, 10, 10) >= 0, "the first glyph is one em at the run's origin");
      CHECK(At(built.Painted, 18, 8, 10, 10) >= 0, "the second follows at the measured advance");
      CHECK(At(built.Painted, 38, 8, 10, 10) >= 0,
            "and the fourth sits past the space, in the column the LAYOUT measured -- the painter "
            "walks the same advances or the two halves disagree about where a word is");
    }
  }

  {
    Built built;
    if (Paint(built, "<style>div{width:5px;height:5px;background:#ff00ff}</style>"
                     "<body><div></div><div></div></body>",
              100, 100)) {
      CHECK(built.Painted.QuadsBeyondTheBound() == 0,
            "a declaration well inside the bound asks for nothing past it");
      CHECK(built.Painted.Quads().size() <= kQuadBound,
            "and the list never exceeds the number somebody chose");
    }
  }

  {
    Built built;
    if (Paint(built, "<style>body{margin:0}p{font-size:10px;line-height:1;margin:0;width:100px;"
                     "color:#000000}</style>"
                     "<body><p>aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa "
                     "aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa aaaaaaaaaa</p></body>",
              200, 400)) {
      size_t tooTall = 0;
      const std::vector<double> starts = PageBreaks(built.Placed, 35, tooTall);
      CHECK(tooTall == 0, "no line is taller than the page, so none had to overflow one");
      CHECK(starts.size() == 4,
            "ten lines of ten pixels in a page of thirty-five make four pages, because the page "
            "holds three whole lines and the fourth begins the next");
      if (starts.size() >= 2) {
        CHECK(starts[0] == 0 && starts[1] == 30,
              "and the break is at a line boundary, not at the page height -- 30 and never 35");
      }

      std::string error;
      Painting second;
      CHECK(second.Build(built.Placed, AhemFont(), error, Page{30, 35}),
            "the second page paints from the same layout, which is laid out once and read through a "
            "window");
      bool aboveTheTarget = false, belowIt = false;
      for (const Quad &quad : second.Quads()) {
        aboveTheTarget = aboveTheTarget || quad.Y < 0;
        belowIt = belowIt || quad.Y >= 35;
      }
      CHECK(!aboveTheTarget && !belowIt,
            "and every quad of it lands inside the page, because the window clipped in the "
            "document's coordinates before the shift was applied");
      CHECK(!second.Quads().empty(), "a page in the middle of the prose is not empty");
    }
  }

  {
    Built built;
    if (Paint(built, "<style>body{margin:0}p{font-size:40px;line-height:1;margin:0;width:400px;"
                     "color:#000000}</style><body><p>aa aa</p></body>",
              400, 400)) {
      size_t tooTall = 0;
      const std::vector<double> starts = PageBreaks(built.Placed, 10, tooTall);
      CHECK(tooTall >= 1, "the line that cannot fit is reported rather than dropped or cut");
      CHECK(!starts.empty(), "and paging still terminates");
    }
  }

  return Report();
}

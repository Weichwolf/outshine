#include <cmath>
#include <string>

#include "Check.h"

#include "Layout.h"
#include "Markup.h"
#include "Style.h"

using outshine::Ui::AhemFont;
using outshine::Ui::Box;
using outshine::Ui::Layout;
using outshine::Ui::Markup;
using outshine::Ui::Stylesheet;

namespace {

struct Laid {
  Markup Tree;
  Stylesheet Sheet;
  Layout Placed;
  AhemFont Font;

  bool Read(const char *markup, const char *css, double width, double height, std::string &why) {
    if (!Tree.Read(markup, why)) { return false; }
    Sheet.Read(Tree.StyleText());
    if (css != nullptr) { Sheet.Read(css); }
    return Placed.Build(Tree, Sheet, width, height, Font, why);
  }
  const Box *WithId(const char *id) const {
    for (const Box &box : Placed.Boxes()) {
      const std::string *found = Tree.AttributeOf(box.Node, "id");
      if (found != nullptr && *found == id) { return &box; }
    }
    return nullptr;
  }
};

bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

}

int main(void) {
  using namespace outshine::Test;

  {
    Laid one;
    std::string why;
    CHECK(one.Read("<body><div id=a style='width:100px;height:20px'></div></body>", nullptr, 800, 600,
                   why),
          "a declaration with nothing but a box places");
    const Box *a = one.WithId("a");
    CHECK(a != nullptr, "the box is there");
    if (a != nullptr) {
      CHECK(Near(a->X, 8) && Near(a->Y, 8), "the body's own margin puts the first box at 8, 8");
      CHECK(Near(a->Width, 100) && Near(a->Height, 20), "and it is the size the author declared");
    }
  }

  {
    Laid one;
    std::string why;
    CHECK(one.Read("<body><div id=a style='width:100px;height:10px;padding:5px;border-width:2px'>"
                   "</div><div id=b style='box-sizing:border-box;width:100px;height:50px;"
                   "padding:5px;border-width:2px'></div></body>",
                   nullptr, 800, 600, why),
          "both boxes place");
    const Box *a = one.WithId("a");
    const Box *b = one.WithId("b");
    CHECK(a != nullptr && b != nullptr, "both are there");
    if (a != nullptr && b != nullptr) {
      CHECK(Near(a->Width, 114) && Near(a->Height, 24),
            "content-box adds the padding and the border to the declared size");
      CHECK(Near(b->Width, 100) && Near(b->Height, 50),
            "border-box declares the outside, so the frame comes out of it");
      CHECK(Near(b->Y, 8 + 24), "and the second block stacks under the first");
    }
  }

  {
    Laid one;
    std::string why;
    CHECK(one.Read("<body><div id=row style='display:flex;width:300px;height:40px;gap:10px'>"
                   "<div id=x style='width:50px'></div>"
                   "<div id=y style='flex-grow:1'></div>"
                   "<div id=z style='width:40px'></div></div></body>",
                   nullptr, 800, 600, why),
          "a row places");
    const Box *x = one.WithId("x");
    const Box *y = one.WithId("y");
    const Box *z = one.WithId("z");
    CHECK(x != nullptr && y != nullptr && z != nullptr, "all three items are there");
    if (x != nullptr && y != nullptr && z != nullptr) {
      CHECK(Near(x->X, 8) && Near(x->Width, 50), "the first item starts at the container's content");
      CHECK(Near(y->X, 8 + 60), "the gap is between the items and not around them");
      CHECK(Near(y->Width, 300 - 50 - 40 - 20),
            "the one item that grows takes every pixel the others left");
      CHECK(Near(z->X, 8 + 300 - 40), "and the last ends where the container does");
      CHECK(Near(x->Height, 40), "align-items stretches an item with no cross size");
    }
  }

  {
    Laid one;
    std::string why;
    CHECK(one.Read("<body><div id=row style='display:flex;width:200px;height:20px;"
                   "justify-content:center;align-items:center'>"
                   "<div id=x style='width:40px;height:10px'></div></div></body>",
                   nullptr, 800, 600, why),
          "a centred row places");
    const Box *x = one.WithId("x");
    CHECK(x != nullptr, "the item is there");
    if (x != nullptr) {
      CHECK(Near(x->X, 8 + 80), "an item is centred on the main axis");
      CHECK(Near(x->Y, 8 + 5), "and on the cross axis");
    }
  }

  {
    Laid one;
    std::string why;
    CHECK(one.Read("<body><div id=col style='display:flex;flex-direction:column;width:100px;"
                   "height:200px'><div id=x style='height:30px'></div>"
                   "<div id=y style='flex-grow:1'></div></div></body>",
                   nullptr, 800, 600, why),
          "a column places");
    const Box *x = one.WithId("x");
    const Box *y = one.WithId("y");
    CHECK(x != nullptr && y != nullptr, "both items are there");
    if (x != nullptr && y != nullptr) {
      CHECK(Near(x->Y, 8) && Near(x->Height, 30), "the first item is at the top at its own height");
      CHECK(Near(y->Y, 38) && Near(y->Height, 170), "and the grower takes the rest of the column");
      CHECK(Near(x->Width, 100), "stretch is the cross axis whichever axis is the main one");
    }
  }

  {
    Laid one;
    std::string why;
    CHECK(one.Read("<body><div id=p style='width:100px;font-size:10px;line-height:1'>"
                   "aaaa bbbb cccc</div></body>",
                   nullptr, 800, 600, why),
          "prose places");
    const Box *p = one.WithId("p");
    CHECK(p != nullptr, "the paragraph is there");
    if (p != nullptr) {
      CHECK(Near(p->Height, 20), "fourteen characters at 10 px wrap onto two lines of 10 px");
      int lines = 0;
      for (const Box &box : one.Placed.Boxes()) {
        if (!box.Text.empty()) { ++lines; }
      }
      CHECK(lines == 2, "and the run became two line boxes rather than one");
    }
  }

  {
    Laid one;
    std::string why;
    CHECK(one.Read("<body><div id=a style='width:50px;height:50px'></div></body>", nullptr, 800, 600,
                   why),
          "the target places");
    const int hit = one.Placed.Hit(20, 20);
    const int missed = one.Placed.Hit(500, 500);
    const std::string *id = one.Tree.AttributeOf(hit, "id");
    CHECK(id != nullptr && *id == "a", "a point inside a box answers that box's own node");
    CHECK(missed < 0, "and a point outside every box answers nothing at all");
  }

  {
    std::string why;
    Laid baseline;
    CHECK(baseline.Read("<div style=\"display:flex; flex-wrap:wrap; align-items:baseline; "
                        "width:85px\">"
                        "<div id=\"tall\" style=\"font-size:40px\">AA</div>"
                        "<div id=\"small\" style=\"font-size:10px\">b</div>"
                        "<div id=\"second\" style=\"font-size:40px\">CC</div>"
                        "<div id=\"third\" style=\"font-size:10px\">d</div></div>",
                        nullptr, 400, 300, why),
          "a WRAPPED baseline-aligned row lays out -- the second line is where the double "
          "cursor read past the line table (board:1685)");
    const Box *tall = baseline.WithId("tall");
    const Box *small = baseline.WithId("small");
    const Box *second = baseline.WithId("second");
    const Box *third = baseline.WithId("third");
    CHECK(tall != nullptr && small != nullptr && small->Y >= tall->Y,
          "the smaller glyph sits lower so the first line's baselines meet");
    CHECK(second != nullptr && third != nullptr &&
              Near(third->Y - second->Y, small->Y - tall->Y),
          "and the SECOND line's baselines meet by the SAME arithmetic as the first "
          "first -- the line's own baseline, read at the line's INDEX, never at its pixel "
          "position (board:1685)");
  }

  Covers("the library measures, wraps and places, and what it answers is where every box "
         "landed in the viewport's own pixels -- judged with no device and no picture");
  return Report();
}

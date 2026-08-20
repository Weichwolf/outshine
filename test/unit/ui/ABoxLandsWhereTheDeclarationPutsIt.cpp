/* WHERE A BOX LANDS, IN PIXELS (board:1442).
 *
 * **The numbers here are the corpus's own idiom.** WPT states a layout case's answer as
 * `data-expected-width` and `data-offset-x` on the element itself -- `board:1443` measures that -- so
 * this test is written the same way: a declaration, and the pixels it must produce. Nothing is compared
 * against a picture, because a layout is right or wrong before anything is painted.
 *
 * **`Ahem` is why the numbers are exact**: every glyph is a solid square of the em, so a run of five
 * characters at 16 px is 80 px and a line is a line rather than a rasterisation. */
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

}  // namespace

int main(void) {
  using namespace outshine::Test;

  /* THE USER-AGENT SHEET IS WHY THE FIRST BOX IS AT EIGHT. Without it every case below is off by
   * `body`'s own margin, which is exactly the eight pixels `board:1443` measured in the corpus. */
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

  /* THE BOX MODEL IN CSS'S OWN DEFAULT: `width` is the CONTENT and the frame is added to it, which is
   * `content-box` -- and `border-box` is the other answer, declared. */
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

  /* FLEX ALONG THE MAIN AXIS: a base, a grow, a gap and a justification. */
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

  /* JUSTIFICATION AND CENTRING, which is the arithmetic a HUD is made of. */
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

  /* A COLUMN IS THE SAME ALGORITHM WITH THE AXES SWAPPED, which is the whole reason it is one. */
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

  /* PROSE, WHICH IS WHAT SEPARATES A QUEST LOG FROM A ROW OF BUTTONS: a run wraps at spaces and every
   * line is one line-height tall. At `Ahem` and 10 px, "aaaa bbbb cccc" is three words of 40 px, and a
   * 100 px column takes two of them on the first line. */
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

  /* A HIT IS A NODE AND NOT A DECISION, which is the whole of what interaction is here. */
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

  Covers("board:1442 the library measures, wraps and places, and what it answers is where every box "
         "landed in the viewport's own pixels -- judged with no device and no picture");
  return Report();
}

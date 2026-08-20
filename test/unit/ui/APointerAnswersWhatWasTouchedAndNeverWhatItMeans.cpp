#include <cstdio>
#include <string>

#include "Check.h"
#include "Layout.h"
#include "Pointer.h"

using namespace outshine::Test;
using namespace outshine::Ui;

namespace {

struct Built {
  Markup Tree;
  Stylesheet Sheet;
  Layout Placed;
};

bool Lay(Built &built, const char *markup, double width, double height) {
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
  return true;
}

}

int main(void) {

  {
    Built built;
    if (Lay(built, "<style>#b{width:120px;height:30px;background:#334455;font-size:10px}"
                   "body{margin:0}</style>"
                   "<body><div id=b data-action=\"open-the-quest-log\">go</div></body>",
            300, 200)) {
      const Touched inside = Under(built.Placed, built.Tree, 60, 15);
      CHECK(inside.Held(), "a point over the element is a hit");
      CHECK(inside.Action == "open-the-quest-log",
            "and the action comes back exactly as declared, opaque to this library");
      CHECK(inside.DeclaredBy == inside.Node,
            "and past the text the element that was hit is the one that declared it");

      const Touched onGlyph = Under(built.Placed, built.Tree, 4, 4);
      CHECK(onGlyph.Action == "open-the-quest-log",
            "a pointer on the first glyph reaches the same declaration");
      CHECK(onGlyph.DeclaredBy >= 0 && onGlyph.DeclaredBy != onGlyph.Node,
            "declared by the ancestor and not by the run that was under the pointer, which is what "
            "makes a declaration cover what it contains");
    }
  }

  {
    Built built;
    if (Lay(built, "<style>body{margin:0}#b{width:10px;height:10px;background:#ffffff}</style>"
                   "<body><div id=b></div></body>",
            300, 200)) {
      const Touched past = Under(built.Placed, built.Tree, 250, 180);
      CHECK(!past.Held(), "a point past every box is a miss and says so");
      CHECK(past.Action.empty(), "and a miss declares nothing");
    }
  }

  {
    Built built;
    if (Lay(built, "<style>body{margin:0}#b{width:40px;height:40px;background:#ffffff}</style>"
                   "<body><div id=b></div></body>",
            300, 200)) {
      const Touched plain = Under(built.Placed, built.Tree, 20, 20);
      CHECK(plain.Held(), "the element is found");
      CHECK(plain.Action.empty() && plain.DeclaredBy < 0,
            "and nothing is invented for it -- an engine that supplied a default action would be an "
            "engine with a vocabulary of meanings");
      CHECK(plain.LocalX == 20 && plain.LocalY == 20,
            "the point inside the border box comes back, so a client can place, drag or seek "
            "without a second lookup");
    }
  }

  {
    Built built;
    if (Lay(built, "<style>body{margin:0}"
                   "#o{width:20px;height:20px;overflow:hidden}"
                   "#i{width:200px;height:200px;background:#112233}</style>"
                   "<body><div id=o data-action=\"outer\"><div id=i data-action=\"inner\"></div>"
                   "</div></body>",
            300, 300)) {
      const Touched visible = Under(built.Placed, built.Tree, 10, 10);
      CHECK(visible.Action == "inner", "inside the clip the child is what the pointer found");
      const Touched clipped = Under(built.Placed, built.Tree, 100, 100);
      CHECK(!clipped.Held(),
            "past the clip the child is not touched, even though its own rectangle covers the point");
    }
  }

  return Report();
}

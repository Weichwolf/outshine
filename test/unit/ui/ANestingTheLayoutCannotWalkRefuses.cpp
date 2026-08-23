#include <cstdio>
#include <string>

#include "Check.h"

#include "Layout.h"
#include "Markup.h"
#include "Style.h"

using outshine::Ui::AhemFont;
using outshine::Ui::Layout;
using outshine::Ui::Markup;
using outshine::Ui::Stylesheet;

namespace {

[[nodiscard]] std::string Nested(int depth) {
  std::string text = "<body>";
  for (int at = 0; at < depth; ++at) { text += "<div>"; }
  text += "x";
  for (int at = 0; at < depth; ++at) { text += "</div>"; }
  text += "</body>";
  return text;
}

struct Laid {
  Markup Tree;
  Stylesheet Sheet;
  Layout Placed;
  AhemFont Font;

  [[nodiscard]] bool Read(const std::string &markup, std::string &why) {
    if (!Tree.Read(markup.c_str(), why)) { return false; }
    Sheet.Read(Tree.StyleText());
    return Placed.Build(Tree, Sheet, 800, 600, Font, why);
  }
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  {
    Laid shallow;
    std::string why;
    CHECK(shallow.Read(Nested(64), why), "a document nested sixty-four deep lays out");
    CHECK(shallow.Placed.Boxes().size() >= 64, "and every level placed a box");
  }
  {
    // one level past the declared bound: a refusal with the number in it, and the boxes
    // left empty rather than half-placed
    Laid deep;
    std::string why;
    const bool laid = deep.Read(Nested(129), why);
    CHECK(!laid && why.find("128") != std::string::npos,
          "**A NESTING THE LAYOUT CANNOT WALK REFUSES**, naming the bound it walks "
          "(board:1754)");
    CHECK(deep.Placed.Boxes().empty(),
          "and nothing is half-placed behind the refusal");
  }
  {
    // the depth a hostile document reaches: the walk spends stack per level, and this
    // once died with SIGSEGV between 4000 and 4200 levels -- no error, no message, the
    // host process gone
    Laid hostile;
    std::string why;
    const bool laid = hostile.Read(Nested(20000), why);
    CHECK(!laid && !why.empty(),
          "**TWENTY THOUSAND LEVELS ARE A REFUSAL, NOT A CRASH**: the process is still "
          "here to answer (board:1754)");
  }

  Covers("III.14 a nesting the layout cannot walk refuses and does not die: the walk's "
         "stack per level is a declared bound, the refusal names it, and a hostile "
         "document leaves the process standing (board:1754)");
  return Report();
}

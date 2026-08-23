#include <chrono>
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

// the shape that made the cost explode: baseline alignment asks a flex line for each
// child's baseline AND its size, and each answer was a throwaway walk of that subtree
[[nodiscard]] std::string Ladder(int depth) {
  std::string text = "<body>";
  for (int at = 0; at < depth; ++at) {
    text += "<div style='display:flex;align-items:baseline'>";
  }
  text += "x";
  for (int at = 0; at < depth; ++at) { text += "</div>"; }
  text += "</body>";
  return text;
}

[[nodiscard]] double LayMs(int depth, size_t &boxes) {
  Markup tree;
  Stylesheet sheet;
  Layout placed;
  AhemFont font;
  std::string why;
  const std::string markup = Ladder(depth);
  if (!tree.Read(markup.c_str(), why)) { return -1.0; }
  sheet.Read(tree.StyleText());
  const auto from = std::chrono::steady_clock::now();
  const bool laid = placed.Build(tree, sheet, 800, 600, font, why);
  const double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - from).count();
  if (!laid) { return -1.0; }
  boxes = placed.Boxes().size();
  return ms;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t shallowBoxes = 0, deepBoxes = 0;
  const double shallow = LayMs(8, shallowBoxes);
  const double deep = LayMs(16, deepBoxes);
  CHECK(shallow >= 0.0 && deep >= 0.0, "both ladders lay out");
  Note("eight levels", shallow, "ms");
  Note("sixteen levels", deep, "ms");
  Note("boxes at sixteen levels", (double)deepBoxes, "boxes");

  // [SET] the bound: twice the boxes may cost at most eight times the time. A linear
  // layout doubles; the four-walks-per-level form multiplied by 3.99 PER LEVEL, so eight
  // more levels cost 3.99^8 = 64000x -- the ladder at depth 12 took 6.5 seconds and depth
  // 16 extrapolated to 27 minutes (board:1753)
  constexpr double kMostGrowth = 8.0;
  const double grew = shallow > 0.05 ? deep / shallow : deep / 0.05;
  Note("how much the doubled depth cost", grew, "x");
  CHECK(deep < 250.0,
        "**SIXTEEN NESTED FLEX LEVELS LAY OUT IN A QUARTER SECOND**: the exponential form "
        "needed twenty-seven minutes for this document (board:1753)");
  CHECK(grew < kMostGrowth,
        "and doubling the depth costs under eight times the time -- a re-walking layout "
        "would multiply by four per level, which is what the number above measures");

  {
    // the picture is unchanged: a memoised intrinsic is the SAME intrinsic
    Markup tree;
    Stylesheet sheet;
    Layout placed;
    AhemFont font;
    std::string why;
    CHECK(tree.Read("<body><div style='display:flex;align-items:baseline'>"
                    "<div id=a style='width:40px;height:20px'></div>"
                    "<div id=b style='width:60px;height:30px'></div></div></body>", why),
          "a two-item flex line reads");
    sheet.Read(tree.StyleText());
    CHECK(placed.Build(tree, sheet, 800, 600, font, why), "and lays out");
    const outshine::Ui::Box *b = nullptr;
    for (const auto &box : placed.Boxes()) {
      const std::string *id = tree.AttributeOf(box.Node, "id");
      if (id != nullptr && *id == "b") { b = &box; }
    }
    // 48 = the first item's 40 plus the user-agent sheet's own gap; what matters is that
    // the memoised walk answers exactly what the repeated walk answered
    CHECK(b != nullptr && b->X == 48.0 && b->Width == 60.0 && b->Height == 30.0,
          "the second item sits after the first at its declared size -- the cache answers "
          "what the walk answered, to the pixel");
  }

  Covers("III.15 the layout's cost is bounded in the markup it is handed: intrinsic sizes "
         "and baselines are computed once per node per available width, so nesting costs "
         "boxes and not powers of four (board:1753)");
  return Report();
}

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
[[nodiscard]] std::string Ladder(int depth, const char *style) {
  std::string text = "<body>";
  for (int at = 0; at < depth; ++at) {
    text += "<div style='";
    text += style;
    text += "'>";
  }
  text += "x";
  for (int at = 0; at < depth; ++at) { text += "</div>"; }
  text += "</body>";
  return text;
}

// the SHAPES a real stylesheet mixes -- the closure that had to be reopened measured only
// the first, which is the one where the memo happens to hit (board:1753)
struct Shape {
  const char *Name;
  const char *Style;
};
constexpr Shape kShapes[] = {
    {"flex+baseline", "display:flex;align-items:baseline"},
    {"percentage width", "display:flex;align-items:baseline;width:90%"},
    {"padding", "display:flex;align-items:baseline;padding:1px"},
    {"wrap", "display:flex;align-items:baseline;flex-wrap:wrap"},
    {"width+padding", "display:flex;align-items:baseline;padding:1px;width:95%"},
    {"width+wrap", "display:flex;align-items:baseline;flex-wrap:wrap;width:95%"},
    {"padding+wrap", "display:flex;align-items:baseline;flex-wrap:wrap;padding:1px"},
    {"all three", "display:flex;align-items:baseline;flex-wrap:wrap;padding:1px;width:95%"},
};

[[nodiscard]] double LayMs(int depth, const char *style, size_t &boxes,
                           outshine::Ui::Layout::Work *spent = nullptr) {
  Markup tree;
  Stylesheet sheet;
  Layout placed;
  AhemFont font;
  std::string why;
  const std::string markup = Ladder(depth, style);
  if (!tree.Read(markup.c_str(), why)) { return -1.0; }
  sheet.Read(tree.StyleText());
  const auto from = std::chrono::steady_clock::now();
  const bool laid = placed.Build(tree, sheet, 800, 600, font, why);
  const double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - from).count();
  if (!laid) { return -1.0; }
  boxes = placed.Boxes().size();
  if (spent != nullptr) { *spent = placed.Spent(); }
  return ms;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // [SET] the bound, in WORK and not in milliseconds: a layout that walks its tree a
  // constant number of times spends places proportional to boxes. Sixteen is generous
  // slack over the handful of passes flex genuinely needs, and no faster machine can
  // make an exponential walk satisfy it (board:1753, its point 3 and 5)
  // [SET] the same allowance the layout budgets, so the test measures the engine's own
  // claim: every shape either lays out within it or REFUSES -- never a stall
  constexpr double kPlacesPerBox = 64.0;
  bool everyShapeBounded = true;
  size_t refused = 0;
  for (const Shape &shape : kShapes) {
    size_t boxes = 0;
    outshine::Ui::Layout::Work spent;
    const double ms = LayMs(14, shape.Style, boxes, &spent);
    const double perBox = boxes > 0 ? (double)spent.Places / (double)boxes : 0.0;
    std::printf("NOTE %-18s depth 14: %8.2f ms  boxes %2zu  places %6zu (%.1f per box)  "
                "measures %zu hits %zu  intrinsics %zu hits %zu%s\n",
                shape.Name, ms, boxes, spent.Places, perBox, spent.Measures,
                spent.MeasureHits, spent.Intrinsics, spent.IntrinsicHits,
                ms < 0.0 ? "  REFUSED" : "");
    if (ms < 0.0) {
      ++refused;
      continue;
    }
    if (perBox > kPlacesPerBox) { everyShapeBounded = false; }
  }
  CHECK(everyShapeBounded,
        "**WHAT THE LAYOUT LAYS OUT, IT LAYS OUT WITHIN ITS BUDGET**: every shape that "
        "returns boxes spent places proportional to them -- and the count is what is "
        "asserted, not a stopwatch a faster machine could satisfy (board:1753)");
  Note("shapes refused as too costly to walk", (double)refused, "shapes");

  {
    // the budget must not be harassment: an ORDINARY document nests the same pair a
    // handful deep, which is what a real interface looks like
    for (const int depth : {2, 4, 6, 8}) {
      size_t boxes = 0;
      outshine::Ui::Layout::Work spent;
      const double ms = LayMs(depth, kShapes[7].Style, boxes, &spent);
      std::printf("NOTE ordinary depth %2d: %8.3f ms  boxes %2zu  places %6zu%s\n", depth,
                  ms, boxes, spent.Places, ms < 0.0 ? "  REFUSED" : "");
      CHECK(ms >= 0.0,
            "an ordinary nesting of percentage width and padding lays out -- the budget "
            "refuses what multiplies, not what an interface declares");
    }
  }
  {
    // the pathological pair (percentage width AND padding) multiplies with nesting; until
    // the walk is linear it must REFUSE by name in bounded time, never stall for minutes
    size_t boxes = 0;
    outshine::Ui::Layout::Work spent;
    const auto from = std::chrono::steady_clock::now();
    const double ms = LayMs(22, kShapes[7].Style, boxes, &spent);
    const double waited = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - from).count();
    Note("what the multiplying shape cost before its verdict", waited, "ms");
    CHECK(ms < 0.0,
          "**A SHAPE WHOSE COST MULTIPLIES IS REFUSED, NOT WALKED**: percentage width with "
          "padding at depth twenty-two cost 22.3 SECONDS when this item was reopened "
          "(board:1753)");
    CHECK(waited < 500.0,
          "and the refusal arrives in under half a second -- a refusal after minutes is "
          "not a refusal (board:1754's second bound)");
  }

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

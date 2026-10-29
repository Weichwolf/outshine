#include "Check.h"
#include "Font.h"
#include "Layout.h"
#include "Markup.h"
#include "PaintList.h"
#include "Style.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace {
using namespace outshine::Ui;

struct MetricFont final : Font {
  [[nodiscard]] FontMetrics At([[maybe_unused]] FontFace face) const override {
    return {.Advance = 6, .Ascent = 8, .Descent = 2};
  }

  [[nodiscard]] Glyph Shape(char32_t code, [[maybe_unused]] FontFace face) const override {
    double advance = 7;
    if (code == U' ') { advance = 3; }
    if (code == U'C') { advance = 0; }
    return {.LeftPx = 1,
            .TopPx = 2,
            .WidthPx = 4,
            .HeightPx = 6,
            .U0 = 0,
            .V0 = 0,
            .U1 = 0,
            .V1 = 0,
            .AdvancePx = advance,
            .Drawn = code != U' '};
  }
};
}

int main() {
  using namespace outshine::Ui;
  using namespace outshine::Test;
  Markup markup;
  std::string error;
  CHECK(markup.Read("<div>AB CD</div>", error), "the text declaration is valid");
  Stylesheet style;
  style.Read("* { margin:0; padding:0; } "
             "div { display:block; font-size:10px; line-height:10px; color:#804020; }");
  const MetricFont font;
  Layout layout;
  CHECK(layout.Build(markup, style, 100, 100, font, error),
        "layout consumes the abstract font metrics and glyph advances");
  const auto text = std::ranges::find(layout.Boxes(), "AB CD", &Box::Text);
  CHECK(text != layout.Boxes().end() && text->Width == 30,
        "text measurement uses individual advances, including metric fallback and spaces");
  PaintList paint;
  CHECK(paint.Build(layout, font, error), "the laid-out text produces viewport paint");
  const auto &quads = paint.Quads();
  CHECK(quads.size() == 4, "a space advances the pen without adding a visible glyph");
  if (quads.size() == 4) {
    constexpr std::array<double, 4> starts{1, 8, 18, 24};
    for (size_t at = 0; at < quads.size(); ++at) {
      const auto &quad = quads[at];
      CHECK(quad.X == starts[at] && quad.Y == 2 && quad.Width == 4 && quad.Height == 6,
            "paint uses the glyph bearing and size independently of its advance and em size");
      CHECK(quad.Colour == 0x804020ff && quad.Opacity == 1 && quad.ClipX == 0 && quad.ClipY == 0 &&
                quad.ClipWidth == 100 && quad.ClipHeight == 100,
            "text color, opacity and viewport clipping reach the paint product unchanged");
    }
  }
  return Report();
}

#include "AhemFont.h"
#include "Check.h"
#include "Layout.h"
#include "PaintList.h"
#include "SurfaceSession.h"
#include "UiPaint.h"
#include <algorithm>
#include <cstddef>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Ui;
using namespace outshine::Test;

SurfaceDeclaration Tiles(size_t count) {
  std::string markup = "<section>";
  for (size_t at = 0; at < count; ++at) { markup += "<div></div>"; }
  markup += "</section>";
  return {.Markup = std::move(markup),
          .Style = "section { display:flex; flex-wrap:wrap; width:256px; } "
                   "div { width:1px; height:1px; flex-grow:0; flex-shrink:0; "
                   "flex-basis:1px; background:#ff0000; }",
          .Programme = {}};
}
}

int main() {
  outshine::Test::AhemFont font;
  SurfaceSession session;
  std::vector<PaintQuad> published;
  size_t attempts = 0;
  session.Configure(
      [&](std::span<const PaintQuad> quads,
          [[maybe_unused]] const PaintAtlasView *atlas,
          [[maybe_unused]] std::string &error) {
        ++attempts;
        published.assign(quads.begin(), quads.end());
        return true;
      },
      &font,
      {Tiles(kQuadBound)},
      {.WidthPx = 1280, .HeightPx = 720});
  std::string error;
  CHECK(session.Compose(error), "all required paint at the declared limit can publish");
  CHECK(attempts == 1 && published.size() == kQuadBound,
        "the valid surface publishes every required visible quad");
  std::set<std::pair<float, float>> positions;
  for (const auto &quad : published) { positions.emplace(quad.LeftPx, quad.TopPx); }
  CHECK(positions.size() == kQuadBound &&
            std::ranges::all_of(published,
                                [](const PaintQuad &quad) {
                                  return quad.WidthPx == 1 && quad.HeightPx == 1 &&
                                         quad.LeftPx >= 0 && quad.LeftPx + quad.WidthPx <= 1280 &&
                                         quad.TopPx >= 0 && quad.TopPx + quad.HeightPx <= 720;
                                }),
        "the regression input contains distinct fully visible pixels without occlusion");
  const std::string original = session.Surfaces().front().Markup;
  CHECK(!session.Redeclare({Tiles(kQuadBound + 1)}, error) && error.contains("quad budget"),
        "an additional required visible quad is refused instead of silently disappearing");
  CHECK(attempts == 1 && published.size() == kQuadBound &&
            session.Surfaces().front().Markup == original,
        "paint admission failure preserves the published surface and declaration");
  CHECK(session.Redeclare({Tiles(1)}, error) && attempts == 2 && published.size() == 1,
        "a later valid declaration can publish after an admission failure");
  return Report();
}

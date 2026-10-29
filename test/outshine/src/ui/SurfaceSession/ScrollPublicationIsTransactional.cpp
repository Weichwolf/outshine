#include "AhemFont.h"
#include "Check.h"
#include "SurfaceSession.h"
#include <cstddef>
#include <span>
#include <string>
#include <vector>

int main() {
  using namespace outshine::Ui;
  using namespace outshine::Test;
  constexpr double sidePx = 64;
  constexpr double stepPx = 32;
  constexpr double hitPx = 16;
  outshine::Test::AhemFont font;
  SurfaceSession session;
  bool accept = true;
  unsigned attempts = 0;
  session.Configure(
      [&]([[maybe_unused]] std::span<const PaintQuad> quads,
          [[maybe_unused]] const PaintAtlasView *atlas,
          std::string &error) {
        ++attempts;
        if (!accept) { error = "injected paint rejection"; }
        return accept;
      },
      &font,
      {{.Markup = "<div><p data-action='first()'></p><p data-action='second()'></p></div>",
        .Style = "div { width:64px; height:32px; overflow:scroll; }"
                 "p { margin:0; width:64px; height:32px; background:#ff0000; }",
        .Programme = {}}},
      {.WidthPx = sidePx, .HeightPx = sidePx});
  std::string error;
  size_t surface = 0;
  const bool composed = session.Compose(error);
  CHECK(composed, "the scrollable CPU-only surface publishes");
  if (!composed) { return Report(); }
  CHECK(session.Under(hitPx, hitPx, surface).Action == "first()",
        "the initial viewport selects the first item");
  const auto initial = session.ScrollState();
  accept = false;
  const auto failed = session.Wheel(hitPx, hitPx, stepPx);
  CHECK(!failed && failed.error() == "injected paint rejection" &&
            session.Under(hitPx, hitPx, surface).Action == "first()" &&
            session.ScrollState().front().empty(),
        "failed wheel publication preserves both scroll state and hit testing");
  accept = true;
  const auto changed = session.Wheel(hitPx, hitPx, stepPx);
  CHECK(changed && *changed && session.Under(hitPx, hitPx, surface).Action == "second()",
        "successful wheel publication moves paint and hit testing together");
  accept = false;
  CHECK(!session.RestoreScroll(initial, error) &&
            session.Under(hitPx, hitPx, surface).Action == "second()" &&
            !session.ScrollState().empty() && !session.ScrollState().front().empty() &&
            session.ScrollState().front().front().Px == stepPx,
        "failed scroll restoration retains the last published viewport");
  accept = true;
  CHECK(session.RestoreScroll(initial, error) &&
            session.Under(hitPx, hitPx, surface).Action == "first()",
        "successful restoration commits the old viewport");
  const unsigned before = attempts;
  const auto outside = session.Wheel(sidePx * 2, sidePx * 2, stepPx);
  CHECK(outside && !*outside && attempts == before,
        "a wheel outside the UI does not invoke publication");
  return Report();
}

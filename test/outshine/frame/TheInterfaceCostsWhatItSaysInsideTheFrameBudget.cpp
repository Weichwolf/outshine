/* WHAT AN INTERFACE COSTS INSIDE 16.67 ms, AND IT IS A DISTRIBUTION OVER A CHANGING DECLARATION
 * (board:1442).
 *
 * **THE SUBJECT IS THE CPU HALF AND THE DOMAIN SAYS SO.** Reading, cascading, measuring, placing and
 * turning a laid-out declaration into rectangles is what this measures; uploading those rectangles and
 * drawing them is the renderer's and is not in this number. A cost quoted without its domain decides
 * nothing, and folding the two would hide which of them moved.
 *
 * **WHY IT IS A FRAME TEST AND NOT A UNIT TEST.** Its subject is a DURATION. A duration measured
 * through a bounds checker is not the shipping frame, and this layer is the one with no sanitiser in
 * the path -- the same split the geometry cost lives under.
 *
 * **THE DECLARATION CHANGES EVERY FRAME AND THAT IS THE POINT.** A HUD whose text never moves would
 * measure a cache and call it a frame: the counters here are re-spelled each iteration, so the
 * stylesheet, the tree and the runs are all genuinely re-read. That is the pessimistic case -- a real
 * consumer would rebuild only when something changed -- and a pessimistic number is the one a budget
 * can be checked against. */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Layout.h"
#include "Paint.h"

using namespace outshine::Test;
using namespace outshine::Ui;

namespace {

/* THE TARGET, AND IT IS THE ONE THIS REPOSITORY IS MEASURED AGAINST EVERYWHERE ELSE. */
constexpr double kFrameBudgetMs = 16.67;

/* [SET] WHAT SHARE OF THE FRAME AN INTERFACE MAY TAKE ON THE CPU. A HUD is not what the frame is FOR:
 * the world, the actors and the picture are, and a tenth is the largest slice that leaves the budget
 * recognisably theirs. It is a number somebody chose, it is here to be argued with, and it is checked
 * rather than quoted. */
constexpr double kInterfaceShare = 0.10;

struct Distribution {
  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0, MinMs = 0.0, MaxMs = 0.0;
};

[[nodiscard]] Distribution Over(std::vector<double> &samples) {
  Distribution out;
  if (samples.empty()) { return out; }
  std::sort(samples.begin(), samples.end());
  const auto at = [&samples](double fraction) {
    return samples[(size_t)(fraction * (double)(samples.size() - 1) + 0.5)];
  };
  out.P50Ms = at(0.50);
  out.P95Ms = at(0.95);
  out.P99Ms = at(0.99);
  out.MinMs = samples.front();
  out.MaxMs = samples.back();
  return out;
}

/* A HUD OF THE SHAPE A GAME ACTUALLY DECLARES: a bar, a row of counters that change, a log of lines
 * that scroll. Everything in it is inside the declared subset, so what is measured is the engine's own
 * path and not a fallback. */
std::string Hud(int frame) {
  std::string document =
      "<style>"
      "body{margin:0;font-size:12px;color:#e8e8e8}"
      ".bar{display:flex;width:1280px;height:28px;background:#101418;"
      "padding:4px;box-sizing:border-box;gap:6px}"
      ".cell{background:#1d242b;padding:2px 6px;flex:0 0 auto;border-width:1px;"
      "border-color:#2e3a44}"
      ".grow{flex:1 1 0%}"
      ".log{width:420px;height:180px;overflow:hidden;background:#0c1013;padding:6px;"
      "box-sizing:border-box;line-height:1.25}"
      "</style><body><div class=bar>";
  for (int i = 0; i < 8; ++i) {
    document += "<div class=cell>" + std::to_string(frame * 7 + i * 131) + "</div>";
  }
  document += "<div class='cell grow'>outshine</div></div><div class=log>";
  for (int line = 0; line < 12; ++line) {
    document += "<p>the courier reached the ridge at " + std::to_string(frame + line) +
                " and reported the road is out past the second bridge</p>";
  }
  document += "</div></body>";
  return document;
}

}  // namespace

int main(void) {
  constexpr int kWarmup = 40;
  constexpr int kFrames = 400;

  std::vector<double> samples;
  samples.reserve(kFrames);
  size_t quads = 0, beyond = 0, boxes = 0;
  const AhemFont font;

  for (int frame = 0; frame < kWarmup + kFrames; ++frame) {
    const std::string document = Hud(frame);
    const auto began = std::chrono::steady_clock::now();

    Markup tree;
    Stylesheet sheet;
    Layout placed;
    Painting painted;
    std::string error;
    const bool read = tree.Read(document, error);
    if (read) {
      sheet.Read(UserAgentSheet());
      sheet.Read(tree.StyleText());
    }
    const bool laid = read && placed.Build(tree, sheet, 1280, 720, font, error);
    const bool drawn = laid && painted.Build(placed, font, error);

    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    if (frame == kWarmup) {
      CHECK(drawn, "the declaration reads, lays out and paints");
      if (!drawn) {
        std::printf("       %s\n", error.c_str());
        return Report();
      }
    }
    if (frame >= kWarmup) {
      samples.push_back(ms);
      quads = painted.Quads().size();
      beyond = painted.QuadsBeyondTheBound();
      boxes = placed.Boxes().size();
    }
  }

  const Distribution cost = Over(samples);
  const double allowed = kFrameBudgetMs * kInterfaceShare;

  /* EVERY NUMBER WITH ITS POPULATION, which here is 400 frames of a declaration that changes on every
   * one of them, at 1280x720, read from source each time. */
  std::printf("NOTE population = %zu frames, %zu boxes, %zu quads at 1280x720, "
              "the declaration re-read every frame\n",
              samples.size(), boxes, quads);
  std::printf("NOTE interface cpu ms  p50 %.4f  p95 %.4f  p99 %.4f  min %.4f  max %.4f\n",
              cost.P50Ms, cost.P95Ms, cost.P99Ms, cost.MinMs, cost.MaxMs);
  std::printf("NOTE budget %.2f ms, share [SET] %.0f%%, allowed %.4f ms, p99 uses %.1f%% of it\n",
              kFrameBudgetMs, kInterfaceShare * 100.0, allowed, 100.0 * cost.P99Ms / allowed);

  /* THE VERDICT IS ON p99 AND NOT ON THE MEAN, because a frame that misses is seen by everyone and an
   * average that holds says nothing about which frame did not. */
  CHECK(cost.P99Ms < allowed,
        "the interface's own path fits inside the share of the frame it was given, at p99");
  CHECK(cost.P50Ms < allowed, "and at p50, which is the case that must not even be close");

  /* THE BOUND IS A NUMBER SOMEBODY CHOSE AND A RUN THAT REACHED IT WOULD BE DRAWING A PICTURE NOBODY
   * DECLARED, so the overage is checked rather than printed. */
  CHECK(beyond == 0, "and it asks for no rectangle past the declared bound");
  CHECK(quads > 0, "having drawn something at all, which is what says the number above is about work");

  return Report();
}

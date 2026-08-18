/* HOW CLOSE THE SUBJECT'S SILHOUETTE COMES TO A PIXEL CENTRE, which is the property that decides
 * whether "the two sides must agree on every pixel" is a fair demand or a wish.
 *
 * A COVERAGE COMPARISON IS A PREDICATE PER PIXEL -- is this centre inside the geometry -- and two
 * implementations answer it identically only where the answer is not on a knife edge. Cycles at
 * `BOX` 0.01 px does not sample the centre: it samples uniformly within +/-0.005 px of it, that
 * being the RNA minimum of `filter_width`. So a pixel whose centre lies closer than 0.005 px to the
 * silhouette is a coin flip on the oracle's side, however exact our own rasteriser is.
 *
 * THE NUMBER THIS FILE PRODUCES IS THEREFORE THE PRECONDITION OF AN EXACTNESS CLAIM, not a score:
 * the smallest distance from a coverage-boundary pixel's centre to any projected edge. Beside it,
 * for a case that disagrees, the same distance at each disagreeing pixel -- which says whether a
 * disagreement is a near-tie the oracle's filter explains or something on our side.
 *
 * THE SILHOUETTE, AND NOT EVERY EDGE, AND THAT WAS MEASURED THE HARD WAY. Taking every triangle edge
 * was argued to reach the same answer, because a boundary pixel's nearest edge is a silhouette edge.
 * It does not: `texture/simple-texture` is an axis-aligned square whose four edges sit half a lattice
 * step from every pixel centre -- a margin of 0.5 px, the largest the construction admits -- and this
 * file reported ZERO for it, because the quad's own interior diagonal runs from (511.5, 487.5) at
 * slope -1 and therefore passes exactly through the pixel centre (512, 487). An interior edge decides
 * no pixel's coverage, so it belongs in neither number here; `Exactness.h` extracts the outline and
 * both numbers below are taken over that. */
#ifndef RENDER_TIES_H
#define RENDER_TIES_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Exactness.h"
#include "Mask.h"
#include "Routing.h"

namespace outshine::Render::Parity {

inline double NearestEdgePx(const EdgeSet &edges, double x, double y) {
  double best = -1.0;
  for (size_t edge = 0; edge < edges.Count(); ++edge) {
    const double ax = edges.Ax[edge], ay = edges.Ay[edge];
    const double dx = edges.Bx[edge] - ax, dy = edges.By[edge] - ay;
    const double squared = dx * dx + dy * dy;
    double along = 0.0;
    if (squared > 0.0) {
      along = ((x - ax) * dx + (y - ay) * dy) / squared;
      along = along < 0.0 ? 0.0 : (along > 1.0 ? 1.0 : along);
    }
    const double ox = x - (ax + along * dx), oy = y - (ay + along * dy);
    const double distance = std::sqrt(ox * ox + oy * oy);
    if (best < 0.0 || distance < best) { best = distance; }
  }
  return best;
}

/* The tie margin of one render: over every pixel that sits on the coverage boundary, the smallest
 * distance from its centre to a projected silhouette edge. A subject whose margin is under the
 * oracle's own filter half-width cannot be asked for pixel-exact agreement. This is the EMPIRICAL
 * reading of what `Exactness.h` predicts from the lines alone, and on a constructed case the two
 * agree to the last digit -- one quantity reached two ways. */
inline double TieMarginPx(const Mask &mask, const EdgeSet &edges) {
  double margin = -1.0;
  for (const Pixel &pixel : Boundary(mask)) {
    const double distance = NearestEdgePx(edges, (double)pixel.X, (double)pixel.Y);
    if (distance >= 0.0 && (margin < 0.0 || distance < margin)) { margin = distance; }
  }
  return margin;
}

/* WHAT THE GEOMETRIC BOUND IS TAKEN OVER: how far the worst pixel of it, and how many there were. A
 * distance quoted without its population is the number this repository has already been broken by --
 * the selection can move underneath it and the metric reads unchanged (board:1144). */
struct WorstDisagreement {
  double Px = 0;      /* the furthest routed pixel -- REPORTED, never the verdict (board:1402) */
  double AtFraction = 0; /* the same distance at the declared fraction of the silhouette */
  size_t Pixels = 0;
};

/* The same distance at the pixels the picture bound ROUTED here, taken at its worst: if even the
 * furthest of them sits inside the oracle's filter half-width, every disagreement in the frame is a
 * near-tie and none of them is evidence about our projection.
 *
 * THE POPULATION IS THE ROUTER'S AND NOT THE COVERAGE MASKS' ALONE (board:1144), and it must be: the
 * picture bound sends a pixel here when the two sides name different surfaces at it as well as when
 * they disagree about covering it, and a pixel routed out of the tail into a metric that did not
 * walk it would be a pixel gated by nothing. Both halves of the population are published by the
 * caller, so the widening is visible in the numbers rather than only in this comment. */
inline WorstDisagreement WorstDisagreementPx(const Routing &routing, const EdgeSet &edges,
                                            size_t silhouette, double fraction) {
  WorstDisagreement out;
  std::vector<double> distances;
  for (int y = 0; y < routing.Ours.Height; ++y) {
    for (int x = 0; x < routing.Ours.Width; ++x) {
      if (!routing.ToGeometry(x, y)) { continue; }
      ++out.Pixels;
      const double distance = NearestEdgePx(edges, (double)x, (double)y);
      if (distance > out.Px) { out.Px = distance; }
      distances.push_back(distance);
    }
  }
  /* THE MAXIMUM IS A CLAIM ABOUT ONE PIXEL AND THE VERDICT IS A CLAIM ABOUT THE SILHOUETTE
   * (board:1402). A pixel that agrees contributes a distance of ZERO to this population -- the whole
   * silhouette is the denominator, not the handful of pixels that happened to disagree -- so a case
   * whose disagreement is smaller than `1 - fraction` of its own outline reads zero here and its
   * maximum is still published beside it.
   *
   * THE FRACTION IS THE PICTURE BOUND'S OWN AND NO NEW NUMBER IS INTRODUCED. [MEASURED] over the
   * corpus the population separates itself and the cut lands in the empty band between the two
   * groups rather than among them: five cases disagree on 1 to 4 samples, which is 0.058 % to
   * 0.183 % of their silhouettes, and the next case up disagrees on 94, which is 4.219 %. **A gap of
   * twenty-three times.** Cases already PASS today carrying up to 29 disagreeing samples while
   * `CompareVolume` fails on ONE -- so the verdict was never a function of how much disagreed, only
   * of how far one sample happened to land from an edge.
   *
   * WHAT THIS CAN STILL MISS IS STATED RATHER THAN WAVED PAST: a hole larger than one per cent of the
   * silhouette passes. That is the owner's own declared bar -- the pictures must look alike to 99 % --
   * and it is the same bar the perceptual half of the comparison already answers to. */
  const size_t allowed = (size_t)((1.0 - fraction) * (double)silhouette);
  if (distances.size() > allowed) {
    std::sort(distances.begin(), distances.end(), std::greater<double>());
    out.AtFraction = distances[allowed];
  }
  return out;
}

} // namespace outshine::Render::Parity
#endif

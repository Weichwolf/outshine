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

#include <cmath>
#include <cstddef>
#include <vector>

#include "Exactness.h"
#include "Mask.h"

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

/* The same distance at the pixels the two sides actually disagree about, taken at its worst: if even
 * the furthest of them sits inside the oracle's filter half-width, every disagreement in the frame
 * is a near-tie and none of them is evidence about our projection. */
inline double WorstDisagreementPx(const Mask &ours, const Mask &oracle, const EdgeSet &edges) {
  double worst = 0.0;
  for (int y = 0; y < ours.Height; ++y) {
    for (int x = 0; x < ours.Width; ++x) {
      if (ours.At(x, y) == oracle.At(x, y)) { continue; }
      const double distance = NearestEdgePx(edges, (double)x, (double)y);
      if (distance > worst) { worst = distance; }
    }
  }
  return worst;
}

} // namespace outshine::Render::Parity
#endif

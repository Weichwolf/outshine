/* WHAT A CAMERA PATH ACTUALLY PUT ON THE SCREEN, sampled OUTSIDE any timed span because a readback
 * is a fence and a copy and would be most of a frame that costs half a millisecond. It is in one
 * place because it is one statement: a timing instrument that could not say its frames drew the
 * subject would report the cost of an empty target as the cost of a picture.
 *
 * COVERAGE IS READ FROM THE DEPTH TARGET AND NOT FROM ALPHA. Under reversed-Z the cleared value is
 * exactly zero and anything drawn is above it, so the depth is a coverage predicate with no shading
 * in it; the scene target's alpha is 1 over the whole frame here and would have counted every one
 * of the 921 600 pixels as covered -- which a first reading of this instrument did, and it divided
 * a per-ray cost by twenty times too many rays.
 *
 * IT IS PIXELS AND NOT FRAGMENTS, and the difference is overdraw: a fragment behind another still
 * traces its ray before the depth test retires it, so a per-ray cost divided by this count is an
 * UPPER bound on what one ray costs. */
#ifndef WHATISDRAWN_H
#define WHATISDRAWN_H

#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

#include "Orbit.h"

#include "GltfStudio.h"
#include "Renderer.h"
#include "Subject.h"

namespace outshine::Test {

/* WHAT THE PATH DREW, AS THREE COUNTS AND NOT ONE. A median alone hides a path that covers the whole
 * frame at one end of its orbit and a corner of it at the other -- two populations under one arm's
 * name, and the p50 of a mixture is a number about neither. The extremes are published so a reader
 * can see it. */
struct Drawn {
  long MedianCoveredPx = 0;
  long LeastCoveredPx = 0;
  long MostCoveredPx = 0;
  double SumRadiance = 0.0;
  /* WHERE A NON-FINITE PIXEL IS AND HOW MANY (board:1413). A sum is NaN the moment one term is, and
   * `nan != nan` made the repeat check report a nondeterminism that was never one -- so the count and
   * the first location travel beside the sum. */
  long NonFinitePx = 0;
  long FirstNonFiniteAt = -1;
  float FirstNonFinite[4] = {0, 0, 0, 0};
  float FirstNonFiniteDepth = 0;
};

[[nodiscard]] inline Drawn WhatThePathDraws(outshine::Render::Renderer &renderer,
                                            const outshine::Gltf::Subject &subject,
                                            const outshine::Gltf::Placement &framed, double scale,
                                            int steps, int probes) {
  Drawn out;
  std::vector<long> counts;
  std::vector<float> depth;
  std::vector<float> linear;
  std::string error;
  for (int probe = 0; probe < probes; ++probe) {
    if (!outshine::Clients::Aim(renderer, subject,
                                OrbitAt(subject, framed, scale, probe * steps / probes, steps),
                                error)) {
      return out;
    }
    /* EVERY PROBE JUMPS THE CAMERA, so every probe is a temporal run of its own (board:1413): a
     * resolve that blended this viewpoint with the last one would report a radiance neither of them
     * has, and this function exists to say what IS drawn. */
    renderer.BeginTemporalRun();
    renderer.RenderFrame();
    if (renderer.ReadDepth(depth) != outshine::Render::ReadState::Ready) { return out; }
    if (renderer.ReadSceneLinear(linear) != outshine::Render::ReadState::Ready) { return out; }
    long covered = 0;
    for (size_t at = 0; at < depth.size() && at * 4u + 3u < linear.size(); ++at) {
      if (depth[at] <= 0.0f) { continue; }
      ++covered;
      const double r = (double)linear[at * 4u], g = (double)linear[at * 4u + 1u],
                   b = (double)linear[at * 4u + 2u];
      if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) {
        ++out.NonFinitePx;
        if (out.FirstNonFiniteAt < 0) {
          out.FirstNonFiniteAt = (long)at;
          for (int ch = 0; ch < 4; ++ch) { out.FirstNonFinite[ch] = linear[at * 4u + (size_t)ch]; }
          out.FirstNonFiniteDepth = depth[at];
        }
        continue;
      }
      out.SumRadiance += r + g + b;
    }
    counts.push_back(covered);
  }
  if (counts.empty()) { return out; }
  std::sort(counts.begin(), counts.end());
  out.MedianCoveredPx = counts[counts.size() / 2];
  out.LeastCoveredPx = counts.front();
  out.MostCoveredPx = counts.back();
  return out;
}

} // namespace outshine::Test
#endif

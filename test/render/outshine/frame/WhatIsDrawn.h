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

struct Drawn {
  long MedianCoveredPx = 0;
  long LeastCoveredPx = 0;
  long MostCoveredPx = 0;
  double SumRadiance = 0.0;

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

}
#endif

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

inline double TieMarginPx(const Mask &mask, const EdgeSet &edges) {
  double margin = -1.0;
  for (const Pixel &pixel : Boundary(mask)) {
    const double distance = NearestEdgePx(edges, (double)pixel.X, (double)pixel.Y);
    if (distance >= 0.0 && (margin < 0.0 || distance < margin)) { margin = distance; }
  }
  return margin;
}

struct WorstDisagreement {
  double Px = 0;
  double AtFraction = 0;
  size_t Pixels = 0;
};

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

  const size_t allowed = (size_t)((1.0 - fraction) * (double)silhouette);
  if (distances.size() > allowed) {
    std::sort(distances.begin(), distances.end(), std::greater<double>());
    out.AtFraction = distances[allowed];
  }
  return out;
}

}
#endif

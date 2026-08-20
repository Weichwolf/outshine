#ifndef RENDER_MASK_H
#define RENDER_MASK_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Metric.h"

namespace outshine::Render::Parity {

struct Mask {
  int Width = 0, Height = 0;
  std::vector<uint8_t> In;

  bool At(int x, int y) const {
    return x >= 0 && y >= 0 && x < Width && y < Height && In[(size_t)y * (size_t)Width + (size_t)x];
  }
  size_t Count() const {
    size_t n = 0;
    for (uint8_t pixel : In) { n += pixel ? 1u : 0u; }
    return n;
  }
  double Fraction() const {
    const double area = (double)Width * (double)Height;
    return area > 0 ? (double)Count() / area : 0.0;
  }
};

struct Pixel {
  int X = 0, Y = 0;
};

inline std::vector<Pixel> Boundary(const Mask &mask) {
  std::vector<Pixel> edge;
  for (int y = 0; y < mask.Height; ++y) {
    for (int x = 0; x < mask.Width; ++x) {
      if (!mask.At(x, y)) { continue; }
      if (mask.At(x - 1, y) && mask.At(x + 1, y) && mask.At(x, y - 1) && mask.At(x, y + 1)) {
        continue;
      }
      edge.push_back({x, y});
    }
  }
  return edge;
}

inline double NearestPx(const Pixel &from, const std::vector<Pixel> &to) {
  double best = -1.0;
  for (const Pixel &other : to) {
    const double dx = (double)(from.X - other.X), dy = (double)(from.Y - other.Y);
    const double squared = dx * dx + dy * dy;
    if (best < 0 || squared < best) { best = squared; }
  }
  return best < 0 ? -1.0 : std::sqrt(best);
}

struct Distribution {
  double P50 = 0, P95 = 0, P99 = 0, Max = 0;
  size_t Samples = 0;
};

inline Distribution BoundaryDisplacement(const Mask &a, const Mask &b) {
  const std::vector<Pixel> edgeA = Boundary(a), edgeB = Boundary(b);
  std::vector<double> distances;
  distances.reserve(edgeA.size() + edgeB.size());
  for (const Pixel &pixel : edgeA) {
    const double d = NearestPx(pixel, edgeB);
    if (d >= 0) { distances.push_back(d); }
  }
  for (const Pixel &pixel : edgeB) {
    const double d = NearestPx(pixel, edgeA);
    if (d >= 0) { distances.push_back(d); }
  }
  std::sort(distances.begin(), distances.end());
  Distribution out;
  out.Samples = distances.size();
  out.P50 = Percentile(distances, 0.50);
  out.P95 = Percentile(distances, 0.95);
  out.P99 = Percentile(distances, 0.99);
  out.Max = distances.empty() ? 0.0 : distances.back();
  return out;
}

inline size_t Disagreeing(const Mask &a, const Mask &b) {
  size_t n = 0;
  for (size_t i = 0; i < a.In.size() && i < b.In.size(); ++i) {
    n += ((a.In[i] != 0) != (b.In[i] != 0)) ? 1u : 0u;
  }
  return n;
}

inline double Iou(const Mask &a, const Mask &b) {
  size_t intersection = 0, uni = 0;
  for (size_t i = 0; i < a.In.size() && i < b.In.size(); ++i) {
    const bool inA = a.In[i] != 0, inB = b.In[i] != 0;
    intersection += (inA && inB) ? 1u : 0u;
    uni += (inA || inB) ? 1u : 0u;
  }
  return uni > 0 ? (double)intersection / (double)uni : 0.0;
}

}
#endif

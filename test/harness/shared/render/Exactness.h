#ifndef RENDER_EXACTNESS_H
#define RENDER_EXACTNESS_H

#include "Aim.h"
#include "Handed.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>


namespace outshine::Render::Parity {

constexpr long kAdmissibleNormSquared = 100;

constexpr double kMarginFloorPx = 0.05;

struct EdgeSet {
  std::vector<double> Ax, Ay, Bx, By;

  size_t Count() const { return Ax.size(); }
};

struct LatticeLine {
  long P = 0, Q = 0;
  double C = 0;
  double SlopeResidualPx = 0;
  double MarginPx = 0;
  double CeilingPx = 0;
  size_t Edges = 0;
};

struct Exactness {
  std::vector<LatticeLine> Lines;
  size_t SilhouetteEdges = 0;

  double SlopeResidualPx = 0;

  double MarginPx = 0;
  double CeilingPx = 0;

  size_t LineCount() const { return Lines.size(); }
};

inline const std::vector<std::pair<long, long>> &AdmissibleSlopes() {
  static const std::vector<std::pair<long, long>> slopes = [] {
    std::vector<std::pair<long, long>> found{{1, 0}};
    for (long q = 1; q * q <= kAdmissibleNormSquared; ++q) {
      for (long p = -10; p <= 10; ++p) {
        if (p * p + q * q > kAdmissibleNormSquared) { continue; }
        if (std::gcd(p < 0 ? -p : p, q) != 1) { continue; }
        found.push_back({p, q});
      }
    }
    return found;
  }();
  return slopes;
}

inline LatticeLine FitLine(double ax, double ay, double bx, double by) {
  LatticeLine best;
  double bestResidual = -1.0;
  const double mx = 0.5 * (ax + bx), my = 0.5 * (ay + by);
  const double dx = bx - ax, dy = by - ay;
  for (const std::pair<long, long> &slope : AdmissibleSlopes()) {
    const double p = (double)slope.first, q = (double)slope.second;
    const double norm = std::sqrt(p * p + q * q);

    const double residual = std::fabs(p * dx - q * dy) / (2.0 * norm);
    if (bestResidual >= 0.0 && residual >= bestResidual) { continue; }
    bestResidual = residual;
    best.P = slope.first;
    best.Q = slope.second;
    best.C = p * mx - q * my;
    best.SlopeResidualPx = residual;
    best.CeilingPx = 0.5 / norm;
    best.MarginPx = std::fabs(best.C - std::round(best.C)) / norm;
  }
  best.Edges = 1;
  return best;
}

namespace Detail {

inline std::vector<uint32_t> WeldedIds(const std::vector<double> &positions) {
  const size_t count = positions.size() / 3;
  std::vector<uint32_t> order(count);
  for (size_t vertex = 0; vertex < count; ++vertex) { order[vertex] = (uint32_t)vertex; }
  const auto before = [&positions](uint32_t a, uint32_t b) {
    for (size_t axis = 0; axis < 3; ++axis) {
      const double left = positions[(size_t)a * 3 + axis], right = positions[(size_t)b * 3 + axis];
      if (left != right) { return left < right; }
    }
    return false;
  };
  std::sort(order.begin(), order.end(), before);
  std::vector<uint32_t> welded(count, 0);
  uint32_t next = 0;
  for (size_t position = 0; position < order.size(); ++position) {
    if (position > 0 && before(order[position - 1], order[position])) { ++next; }
    welded[order[position]] = next;
  }
  return welded;
}

struct Adjacency {
  uint32_t Low = 0, High = 0;
  uint32_t Triangle = 0;
  int8_t Facing = 0;
};

}

inline EdgeSet Silhouette(const outshine::Test::Handed &subject, const outshine::Test::Clip &clip,
                          const outshine::Test::Frame &viewport) {
  const std::vector<uint32_t> &indices = subject.Indices();
  const std::vector<double> &positions = subject.PositionsM();
  const size_t vertices = positions.size() / 3;

  std::vector<double> raster(vertices * 2, 0.0);
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const double point[3] = {positions[vertex * 3], positions[vertex * 3 + 1],
                             positions[vertex * 3 + 2]};
    double ndc[3];
    clip.Point(point, ndc);
    viewport.Raster(ndc, &raster[vertex * 2]);
  }

  const std::vector<uint32_t> welded = Detail::WeldedIds(positions);
  std::vector<Detail::Adjacency> edges;
  edges.reserve(indices.size());
  for (size_t triangle = 0; triangle * 3 + 2 < indices.size(); ++triangle) {
    const uint32_t corner[3] = {indices[triangle * 3], indices[triangle * 3 + 1],
                                indices[triangle * 3 + 2]};
    const double *a = &raster[(size_t)corner[0] * 2];
    const double *b = &raster[(size_t)corner[1] * 2];
    const double *c = &raster[(size_t)corner[2] * 2];
    const double twiceArea = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
    const int8_t facing = twiceArea > 0.0 ? (int8_t)1 : (twiceArea < 0.0 ? (int8_t)-1 : (int8_t)0);
    for (int side = 0; side < 3; ++side) {
      const uint32_t u = welded[corner[side]], v = welded[corner[(side + 1) % 3]];
      edges.push_back({u < v ? u : v, u < v ? v : u, (uint32_t)triangle, facing});
    }
  }
  std::sort(edges.begin(), edges.end(), [](const Detail::Adjacency &a, const Detail::Adjacency &b) {
    return a.Low != b.Low ? a.Low < b.Low : a.High < b.High;
  });

  EdgeSet silhouette;
  for (size_t start = 0; start < edges.size();) {
    size_t stop = start + 1;
    while (stop < edges.size() && edges[stop].Low == edges[start].Low &&
           edges[stop].High == edges[start].High) {
      ++stop;
    }
    bool outline = (stop - start) == 1;
    for (size_t at = start + 1; at < stop && !outline; ++at) {
      outline = edges[at].Facing != edges[start].Facing;
    }
    if (outline) {

      const Detail::Adjacency &edge = edges[start];
      size_t endpointA = 0, endpointB = 0;
      for (int side = 0; side < 3; ++side) {
        const uint32_t vertex = indices[(size_t)edge.Triangle * 3 + (size_t)side];
        if (welded[vertex] == edge.Low) { endpointA = vertex; }
        if (welded[vertex] == edge.High) { endpointB = vertex; }
      }
      silhouette.Ax.push_back(raster[endpointA * 2]);
      silhouette.Ay.push_back(raster[endpointA * 2 + 1]);
      silhouette.Bx.push_back(raster[endpointB * 2]);
      silhouette.By.push_back(raster[endpointB * 2 + 1]);
    }
    start = stop;
  }
  return silhouette;
}

inline Exactness Measure(const EdgeSet &silhouette) {
  Exactness measured;
  measured.SilhouetteEdges = silhouette.Count();
  std::vector<LatticeLine> fitted;
  fitted.reserve(silhouette.Count());
  for (size_t edge = 0; edge < silhouette.Count(); ++edge) {
    fitted.push_back(FitLine(silhouette.Ax[edge], silhouette.Ay[edge], silhouette.Bx[edge],
                             silhouette.By[edge]));
  }

  std::sort(fitted.begin(), fitted.end(), [](const LatticeLine &a, const LatticeLine &b) {
    if (a.P != b.P) { return a.P < b.P; }
    if (a.Q != b.Q) { return a.Q < b.Q; }
    return a.C < b.C;
  });
  constexpr double kSameLinePx = 1e-6;
  for (const LatticeLine &fit : fitted) {
    LatticeLine *last = measured.Lines.empty() ? nullptr : &measured.Lines.back();
    if (last != nullptr && last->P == fit.P && last->Q == fit.Q &&
        std::fabs(last->C - fit.C) <= kSameLinePx) {
      ++last->Edges;
      last->SlopeResidualPx = std::max(last->SlopeResidualPx, fit.SlopeResidualPx);
      continue;
    }
    measured.Lines.push_back(fit);
  }

  for (size_t line = 0; line < measured.Lines.size(); ++line) {
    const LatticeLine &fit = measured.Lines[line];
    measured.SlopeResidualPx = std::max(measured.SlopeResidualPx, fit.SlopeResidualPx);
    if (line == 0 || fit.MarginPx < measured.MarginPx) { measured.MarginPx = fit.MarginPx; }
    if (line == 0 || fit.CeilingPx < measured.CeilingPx) { measured.CeilingPx = fit.CeilingPx; }
  }
  return measured;
}

}
#endif

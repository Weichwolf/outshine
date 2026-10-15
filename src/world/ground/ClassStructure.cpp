#include "math/Vec2.h"
#include "ClassStructure.h"
#include "ClassSeed.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace outshine {

constexpr unsigned kWindShift = 24u;

constexpr int kSignedByteBias = 128;

constexpr uint32_t kFullByte = 0xFF;

constexpr uint32_t kByteMask = 0xFFu;

namespace {

struct Edge {
  Vec2f From;
  Vec2f To;
};

struct Span {
  double Least = 0.0;
  double Most = 0.0;
};

[[nodiscard]] inline Edge EdgeAt(const float *p) {
  return {.From = {{p[0], p[1]}}, .To = {{p[2], p[3]}}};
}

inline int CrossX(Edge of, double cy, Span within) {
  const double x0 = of.From[0];
  const double y0 = of.From[1];
  const double x1 = of.To[0];
  const double y1 = of.To[1];
  if ((y0 <= cy) == (y1 <= cy)) { return 0; }
  const double xi = x0 + (cy - y0) * (x1 - x0) / (y1 - y0);
  if (xi < within.Least || xi >= within.Most) { return 0; }
  return y1 > y0 ? 1 : -1;
}

inline int CrossY(Edge of, double cx, Span within) {
  const double x0 = of.From[0];
  const double y0 = of.From[1];
  const double x1 = of.To[0];
  const double y1 = of.To[1];
  if ((x0 <= cx) == (x1 <= cx)) { return 0; }
  const double yi = y0 + (cx - x0) * (y1 - y0) / (x1 - x0);
  if (yi <= within.Least || yi > within.Most) { return 0; }
  return x1 > x0 ? -1 : 1;
}

double SegDist(Vec2 from, Edge of) {
  const double px = from[0];
  const double py = from[1];
  const double x0 = of.From[0];
  const double y0 = of.From[1];
  const double dx = static_cast<double>(of.To[0]) - x0;
  const double dy = static_cast<double>(of.To[1]) - y0;
  const double l2 = dx * dx + dy * dy;
  double t = l2 > 0.0 ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0.0;
  t = std::clamp(t, 0.0, 1.0);
  const double qx = x0 + t * dx - px;
  const double qy = y0 + t * dy - py;
  return std::sqrt(qx * qx + qy * qy);
}

struct SeedSample {
  int Class;
  int Rank;
  bool Inside;
  double SignedDistanceM;
};

SeedSample SampleSeed(const ClassStructure::Grid &grid, size_t seed, Vec2 at, Vec2 corner) {
  const auto labels = grid.Seeds[seed];
  const auto first = grid.Seeds[seed + 1];
  const auto count = grid.Seeds[seed + 2];
  const auto halfWidthM = std::bit_cast<float>(grid.Seeds[seed + 3]);
  int winding = static_cast<int>((labels >> kWindShift) & kByteMask) - kSignedByteBias;
  double distance = ClassStructure::kNoEdgeM;
  for (uint32_t r = 0; r < count; ++r) {
    const auto edge = EdgeAt(&grid.Edges[static_cast<size_t>(grid.Refs[first + r]) * 4]);
    if (halfWidthM <= 0) {
      winding += CrossX(edge, corner[1], {.Least = corner[0], .Most = at[0]});
      winding += CrossY(edge, at[0], {.Least = corner[1], .Most = at[1]});
    }
    distance = std::min(distance, SegDist(at, edge));
  }
  const bool inside = halfWidthM > 0 ? distance <= halfWidthM : winding != 0;
  double signedDistance = inside ? distance : -distance;
  if (halfWidthM > 0) { signedDistance = halfWidthM - distance; }
  return {.Class = static_cast<int>(labels & kByteMask),
          .Rank = static_cast<int>((labels >> 8u) & kByteMask),
          .Inside = inside,
          .SignedDistanceM = signedDistance};
}

}

ClassStructure::ClassStructure(const TangentFrame &frame,
                               std::shared_ptr<const Grid> fine,
                               std::shared_ptr<const Grid> coarse,
                               FromRun of)
    : Frame_(frame),
      Fine_(std::move(fine)),
      Coarse_(std::move(coarse)),
      UnmappedRow_(of.UnmappedRow),
      Version_(of.Version) {
  Measures_.BuildMs = of.BuildMs;
  Measures_.Overflow = of.Overflow;
  Measures_.Edges = static_cast<long>((Fine_->Edges.size() + Coarse_->Edges.size()) / 4);
  Measures_.Seeds =
      static_cast<long>((Fine_->Seeds.size() + Coarse_->Seeds.size()) / kClassSeedWords);
  Probe();
}

void ClassStructure::Probe() {
  const Grid &B = *Fine_;
  for (int j = 0; j < B.H; j++) {
    for (int i = 0; i < B.W; i++) {
      const double e = B.OrgE + (static_cast<double>(i) + 0.5) * B.CellM;
      const double n = B.OrgN + (static_cast<double>(j) + 0.5) * B.CellM;
      Measures_.Probes++;
      if (Evaluate(e, n, nullptr, nullptr) < 0) { Measures_.NoData++; }
    }
  }
}

int ClassStructure::Evaluate(double e, double n, double *distM, int *runnerUp) const {
  Sample sample;
  if (std::isfinite(e) && std::isfinite(n)) {
    for (const auto *grid : {Fine_.get(), Coarse_.get()}) {
      sample = EvaluateGrid(*grid, {{e, n}});
      if (sample.Class >= 0) { break; }
    }
  }
  if (distM != nullptr) { *distM = sample.DistanceM; }
  if (runnerUp != nullptr) { *runnerUp = sample.RunnerUp; }
  return sample.Class;
}

ClassStructure::Sample ClassStructure::EvaluateGrid(const Grid &B, Vec2 at) {
  const double e = at[0];
  const double n = at[1];
  if (B.W == 0 || B.H == 0 || e < B.OrgE || n < B.OrgN || e >= B.OrgE + B.W * B.CellM ||
      n >= B.OrgN + B.H * B.CellM) {
    return {};
  }
  const int i = static_cast<int>(std::floor((e - B.OrgE) / B.CellM));
  const int j = static_cast<int>(std::floor((n - B.OrgN) / B.CellM));
  if (i < 0 || j < 0 || i >= B.W || j >= B.H) { return {}; }
  int best = -1;
  int bestRank = -1;
  int second = -1;
  int secondRank = -1;
  double bestDist = kNoEdgeM;
  const size_t ci = (static_cast<size_t>(j) * B.W + static_cast<size_t>(i)) * 2;
  const uint32_t c0 = B.Cells[ci];
  const uint32_t nseed = (c0 >> 16u) & kByteMask;
  const uint32_t seedFirst = B.Cells[ci + 1];
  if ((c0 & kByteMask) != kFullByte) {
    best = static_cast<int>(c0 & kByteMask);
    bestRank = static_cast<int>((c0 >> 8u) & kByteMask);
  }
  const double cx = B.OrgE + static_cast<double>(i) * B.CellM;
  const double cy = B.OrgN + static_cast<double>(j) * B.CellM;
  for (uint32_t s = 0; s < nseed; s++) {
    const size_t seed = static_cast<size_t>(seedFirst + s) * kClassSeedWords;
    const auto candidate = SampleSeed(B, seed, at, {{cx, cy}});
    if (!candidate.Inside) { continue; }
    if (candidate.Rank > bestRank) {
      second = best;
      secondRank = bestRank;
      best = candidate.Class;
      bestRank = candidate.Rank;
      bestDist = candidate.SignedDistanceM;
    } else if (candidate.Rank > secondRank) {
      second = candidate.Class;
      secondRank = candidate.Rank;
    }
  }
  return {.Class = best, .Rank = bestRank, .RunnerUp = second, .DistanceM = bestDist};
}

ClassStructure::MaterialBlend
ClassStructure::MaterialForGrid(const Grid &grid, Vec2 at, Sample sample) {
  MaterialBlend result{.Class = sample.Class, .HaloM = grid.BoundaryHaloM};
  if (grid.BoundaryHaloM <= 0) { return result; }
  const int i = static_cast<int>(std::floor((at[0] - grid.OrgE) / grid.CellM));
  const int j = static_cast<int>(std::floor((at[1] - grid.OrgN) / grid.CellM));
  const size_t cell = (static_cast<size_t>(j) * grid.W + static_cast<size_t>(i)) * 2;
  const auto count = (grid.Cells[cell] >> 16u) & kByteMask;
  const auto first = grid.Cells[cell + 1];
  const double cornerE = grid.OrgE + i * grid.CellM;
  const double cornerN = grid.OrgN + j * grid.CellM;
  int edgeRank = -1;
  for (uint32_t s = 0; s < count; ++s) {
    const size_t seed = static_cast<size_t>(first + s) * kClassSeedWords;
    const auto labels = grid.Seeds[seed];
    const int rank = static_cast<int>((labels >> 8u) & kByteMask);
    if (rank < sample.Rank || rank < edgeRank) { continue; }
    const auto candidate = SampleSeed(grid, seed, at, {{cornerE, cornerN}});
    const double signedDistance = candidate.SignedDistanceM;
    if (std::abs(signedDistance) > grid.BoundaryHaloM ||
        (rank == edgeRank && std::abs(signedDistance) >= std::abs(result.SignedEdgeM))) {
      continue;
    }
    result.Class = candidate.Class;
    result.Other = candidate.Inside ? sample.RunnerUp : sample.Class;
    result.SignedEdgeM = signedDistance;
    edgeRank = rank;
  }
  if (result.Other == result.Class) { return {.Class = sample.Class, .HaloM = grid.BoundaryHaloM}; }
  return result;
}

ClassStructure::MaterialBlend
ClassStructure::MaterialAt(double e, double n, double halfFootprintM) const {
  if (!std::isfinite(e) || !std::isfinite(n) || !std::isfinite(halfFootprintM) ||
      halfFootprintM < 0) {
    return {};
  }
  for (const auto *grid : {Fine_.get(), Coarse_.get()}) {
    if (grid->W <= 0 || grid->H <= 0 || e < grid->OrgE || n < grid->OrgN ||
        e >= grid->OrgE + grid->W * grid->CellM || n >= grid->OrgN + grid->H * grid->CellM) {
      continue;
    }
    if (grid->BoundaryHaloM > 0 && halfFootprintM > .5 * grid->CellM) { continue; }
    const auto sample = EvaluateGrid(*grid, {{e, n}});
    const auto result = MaterialForGrid(*grid, {{e, n}}, sample);
    if (sample.Class >= 0 || result.SignedEdgeM < kNoEdgeM) { return result; }
  }
  return {};
}

}

#include "math/Vec2.h"
#include "ClassStructure.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace outshine {

constexpr double kMsPerMicrosecond = 1e-3;

constexpr unsigned kWindShift = 24u;

constexpr int kSignedByteBias = 128;

constexpr uint32_t kFullByte = 0xFF;

constexpr uint32_t kByteMask = 0xFFu;

namespace {

double Clock() {
  using namespace std::chrono;
  return static_cast<double>(
             duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) *
         kMsPerMicrosecond;
}

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
  if (yi < within.Least || yi >= within.Most) { return 0; }
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

} // namespace

ClassStructure::ClassStructure(const TangentFrame &frame,
                               std::shared_ptr<const Grid> fine,
                               std::shared_ptr<const Grid> coarse,
                               FromRun of)
    : Frame_(frame), Fine_(std::move(fine)), Coarse_(std::move(coarse)), Version_(of.Version) {
  const double t0 = Clock();
  Pack(of.UnmappedRow);
  Measures_.PackMs = Clock() - t0;
  Measures_.BuildMs = of.BuildMs;
  Measures_.Overflow = of.Overflow;
  Measures_.Edges = static_cast<long>((Fine_->Edges.size() + Coarse_->Edges.size()) / 4);
  Measures_.Seeds = static_cast<long>((Fine_->Seeds.size() + Coarse_->Seeds.size()) / 3);
  Probe();
}

void ClassStructure::Pack(int unmappedRow) {
  constexpr uint32_t kHead = 4;
  constexpr uint32_t kHdrWords = 12;
  Words_.assign(kHead + 2u * kHdrWords, 0u);
  Words_[2] = static_cast<uint32_t>(unmappedRow);
  const std::array<const Grid *, 2> grids = {Fine_.get(), Coarse_.get()};
  for (uint32_t b = 0; b < 2; b++) {
    const Grid &B = *grids[b];
    const uint32_t h = kHead + b * kHdrWords;
    Words_[b] = B.W > 0 ? h : 0u;
    if (B.W == 0) { continue; }
    Words_[h + 0] = static_cast<uint32_t>(B.W);
    Words_[h + 1] = static_cast<uint32_t>(B.H);
    const auto oe = static_cast<float>(B.OrgE);
    const auto on = static_cast<float>(B.OrgN);
    const auto cm = static_cast<float>(B.CellM);
    std::memcpy(&Words_[h + 2], &oe, 4);
    std::memcpy(&Words_[h + 3], &on, 4);
    std::memcpy(&Words_[h + 4], &cm, 4);
    Words_[h + 5] = static_cast<uint32_t>(Words_.size());
    Words_.insert(Words_.end(), B.Cells.begin(), B.Cells.end());
    Words_[h + 6] = static_cast<uint32_t>(Words_.size());
    Words_.insert(Words_.end(), B.Seeds.begin(), B.Seeds.end());
    Words_[h + 7] = static_cast<uint32_t>(Words_.size());
    Words_.insert(Words_.end(), B.Refs.begin(), B.Refs.end());
    Words_[h + 8] = static_cast<uint32_t>(Words_.size());
    const size_t at = Words_.size();
    Words_.resize(at + B.Edges.size());
    std::memcpy(&Words_[at], B.Edges.data(), B.Edges.size() * 4);
  }
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
  int best = -1;
  int bestRank = -1;
  int second = -1;
  int secondRank = -1;
  double bestDist = kNoEdgeM;
  const std::array<const Grid *, 2> grids = {Fine_.get(), Coarse_.get()};
  for (const auto &grid : grids) {
    const Grid &B = *grid;
    if (B.W == 0) { continue; }
    const int i = static_cast<int>(std::floor((e - B.OrgE) / B.CellM));
    const int j = static_cast<int>(std::floor((n - B.OrgN) / B.CellM));
    if (i < 0 || j < 0 || i >= B.W || j >= B.H) { continue; }
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
      const uint32_t w0 = B.Seeds[static_cast<size_t>(seedFirst + s) * 3];
      const uint32_t refFirst = B.Seeds[(seedFirst + s) * 3 + 1];
      float halfW;
      std::memcpy(&halfW, &B.Seeds[(seedFirst + s) * 3 + 2], sizeof halfW);
      const int tpl = static_cast<int>(w0 & kByteMask);
      const int rank = static_cast<int>((w0 >> 8u) & kByteMask);
      const uint32_t nref = (w0 >> 16u) & kByteMask;
      int wind = static_cast<int>((w0 >> kWindShift) & kByteMask) - kSignedByteBias;
      double d = kNoEdgeM;
      for (uint32_t r = 0; r < nref; r++) {
        const float *p = &B.Edges[static_cast<size_t>(B.Refs[refFirst + r]) * 4];
        if (halfW <= 0.0f) {
          wind += CrossX(EdgeAt(p), cy, {.Least = cx, .Most = e});
          wind += CrossY(EdgeAt(p), e, {.Least = cy, .Most = n});
        }
        d = std::min(d, SegDist({{e, n}}, EdgeAt(p)));
      }
      if (halfW > 0.0f) {
        if (d > halfW) { continue; }
        d = static_cast<double>(halfW) - d;
      } else if (wind == 0) {
        continue;
      }
      if (rank > bestRank) {
        second = best;
        secondRank = bestRank;
        best = tpl;
        bestRank = rank;
        bestDist = d;
      } else if (rank > secondRank) {
        second = tpl;
        secondRank = rank;
      }
    }
    if (best >= 0) { break; }
  }
  if (distM != nullptr) { *distM = bestDist; }
  if (runnerUp != nullptr) { *runnerUp = second; }
  return best;
}

} // namespace outshine

#include "ClassStructure.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace outshine {

namespace {

double Clock() {
  using namespace std::chrono;
  return static_cast<double>(
             duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) *
         1e-3;
}

inline int CrossX(float x0, float y0, float x1, float y1, double cy, double xa, double xb) {
  if ((y0 <= cy) == (y1 <= cy)) { return 0; }
  const double xi =
      static_cast<double>(x0) + (cy - static_cast<double>(y0)) *
                                    (static_cast<double>(x1) - static_cast<double>(x0)) /
                                    (static_cast<double>(y1) - static_cast<double>(y0));
  if (xi < xa || xi >= xb) { return 0; }
  return y1 > y0 ? 1 : -1;
}

inline int CrossY(float x0, float y0, float x1, float y1, double cx, double ya, double yb) {
  if ((x0 <= cx) == (x1 <= cx)) { return 0; }
  const double yi =
      static_cast<double>(y0) + (cx - static_cast<double>(x0)) *
                                    (static_cast<double>(y1) - static_cast<double>(y0)) /
                                    (static_cast<double>(x1) - static_cast<double>(x0));
  if (yi < ya || yi >= yb) { return 0; }
  return x1 > x0 ? -1 : 1;
}

double SegDist(double px, double py, float x0, float y0, float x1, float y1) {
  const double dx = static_cast<double>(x1) - x0;
  const double dy = static_cast<double>(y1) - y0;
  const double l2 = dx * dx + dy * dy;
  double t = l2 > 0.0 ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0.0;
  t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
  const double qx = x0 + t * dx - px;
  const double qy = y0 + t * dy - py;
  return std::sqrt(qx * qx + qy * qy);
}

} // namespace

ClassStructure::ClassStructure(const TangentFrame &frame,
                               std::shared_ptr<const Grid> fine,
                               std::shared_ptr<const Grid> coarse,
                               uint64_t version,
                               int unmappedRow,
                               double buildMs,
                               int overflow)
    : Frame_(frame), Fine_(std::move(fine)), Coarse_(std::move(coarse)), Version_(version) {
  const double t0 = Clock();
  Pack(unmappedRow);
  Measures_.PackMs = Clock() - t0;
  Measures_.BuildMs = buildMs;
  Measures_.Overflow = overflow;
  Measures_.Edges = static_cast<long>((Fine_->Edges.size() + Coarse_->Edges.size()) / 4);
  Measures_.Seeds = static_cast<long>((Fine_->Seeds.size() + Coarse_->Seeds.size()) / 3);
  Probe();
}

void ClassStructure::Pack(int unmappedRow) {
  constexpr uint32_t kHead = 4;
  constexpr uint32_t kHdrWords = 12;
  Words_.assign(kHead + 2u * kHdrWords, 0u);
  Words_[2] = static_cast<uint32_t>(unmappedRow);
  const Grid *grids[2] = {Fine_.get(), Coarse_.get()};
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
  const Grid *grids[2] = {Fine_.get(), Coarse_.get()};
  for (auto &grid : grids) {
    const Grid &B = *grid;
    if (B.W == 0) { continue; }
    const int i = static_cast<int>(std::floor((e - B.OrgE) / B.CellM));
    const int j = static_cast<int>(std::floor((n - B.OrgN) / B.CellM));
    if (i < 0 || j < 0 || i >= B.W || j >= B.H) { continue; }
    const size_t ci = (static_cast<size_t>(j) * B.W + static_cast<size_t>(i)) * 2;
    const uint32_t c0 = B.Cells[ci];
    const uint32_t nseed = (c0 >> 16u) & 0xFFu;
    const uint32_t seedFirst = B.Cells[ci + 1];
    if ((c0 & 0xFFu) != 0xFF) {
      best = static_cast<int>(c0 & 0xFFu);
      bestRank = static_cast<int>((c0 >> 8u) & 0xFFu);
    }
    const double cx = B.OrgE + static_cast<double>(i) * B.CellM;
    const double cy = B.OrgN + static_cast<double>(j) * B.CellM;
    for (uint32_t s = 0; s < nseed; s++) {
      const uint32_t w0 = B.Seeds[(seedFirst + s) * 3];
      const uint32_t refFirst = B.Seeds[(seedFirst + s) * 3 + 1];
      float halfW;
      std::memcpy(&halfW, &B.Seeds[(seedFirst + s) * 3 + 2], sizeof halfW);
      const int tpl = static_cast<int>(w0 & 0xFFu);
      const int rank = static_cast<int>((w0 >> 8u) & 0xFFu);
      const uint32_t nref = (w0 >> 16u) & 0xFFu;
      int wind = static_cast<int>((w0 >> 24u) & 0xFFu) - 128;
      double d = kNoEdgeM;
      for (uint32_t r = 0; r < nref; r++) {
        const float *p = &B.Edges[static_cast<size_t>(B.Refs[refFirst + r]) * 4];
        if (halfW <= 0.0f) {
          wind += CrossX(p[0], p[1], p[2], p[3], cy, cx, e);
          wind += CrossY(p[0], p[1], p[2], p[3], e, cy, n);
        }
        d = std::min(d, SegDist(e, n, p[0], p[1], p[2], p[3]));
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

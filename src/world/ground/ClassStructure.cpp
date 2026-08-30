#include "ClassStructure.h"

#include <chrono>
#include <cmath>
#include <cstring>

namespace outshine {

namespace {

double Clock() {
  using namespace std::chrono;
  return (double)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count() * 1e-3;
}

inline int CrossX(float x0, float y0, float x1, float y1, double cy, double xa, double xb) {
  if ((y0 <= cy) == (y1 <= cy)) { return 0; }
  const double xi =
      (double)x0 + (cy - (double)y0) * ((double)x1 - (double)x0) / ((double)y1 - (double)y0);
  if (xi < xa || xi >= xb) { return 0; }
  return y1 > y0 ? 1 : -1;
}

inline int CrossY(float x0, float y0, float x1, float y1, double cx, double ya, double yb) {
  if ((x0 <= cx) == (x1 <= cx)) { return 0; }
  const double yi =
      (double)y0 + (cx - (double)x0) * ((double)y1 - (double)y0) / ((double)x1 - (double)x0);
  if (yi < ya || yi >= yb) { return 0; }
  return x1 > x0 ? -1 : 1;
}

double SegDist(double px, double py, float x0, float y0, float x1, float y1) {
  const double dx = (double)x1 - x0, dy = (double)y1 - y0;
  const double l2 = dx * dx + dy * dy;
  double t = l2 > 0.0 ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0.0;
  t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
  const double qx = x0 + t * dx - px, qy = y0 + t * dy - py;
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
  Measures_.Edges = (long)((Fine_->Edges.size() + Coarse_->Edges.size()) / 4);
  Measures_.Seeds = (long)((Fine_->Seeds.size() + Coarse_->Seeds.size()) / 3);
  Probe();
}

void ClassStructure::Pack(int unmappedRow) {
  constexpr uint32_t kHead = 4;
  constexpr uint32_t kHdrWords = 12;
  Words_.assign(kHead + 2u * kHdrWords, 0u);
  Words_[2] = (uint32_t)unmappedRow;
  const Grid *grids[2] = {Fine_.get(), Coarse_.get()};
  for (uint32_t b = 0; b < 2; b++) {
    const Grid &B = *grids[b];
    const uint32_t h = kHead + b * kHdrWords;
    Words_[b] = B.W > 0 ? h : 0u;
    if (B.W == 0) { continue; }
    Words_[h + 0] = (uint32_t)B.W;
    Words_[h + 1] = (uint32_t)B.H;
    const float oe = (float)B.OrgE, on = (float)B.OrgN, cm = (float)B.CellM;
    std::memcpy(&Words_[h + 2], &oe, 4);
    std::memcpy(&Words_[h + 3], &on, 4);
    std::memcpy(&Words_[h + 4], &cm, 4);
    Words_[h + 5] = (uint32_t)Words_.size();
    Words_.insert(Words_.end(), B.Cells.begin(), B.Cells.end());
    Words_[h + 6] = (uint32_t)Words_.size();
    Words_.insert(Words_.end(), B.Seeds.begin(), B.Seeds.end());
    Words_[h + 7] = (uint32_t)Words_.size();
    Words_.insert(Words_.end(), B.Refs.begin(), B.Refs.end());
    Words_[h + 8] = (uint32_t)Words_.size();
    const size_t at = Words_.size();
    Words_.resize(at + B.Edges.size());
    std::memcpy(&Words_[at], B.Edges.data(), B.Edges.size() * 4);
  }
}

void ClassStructure::Probe() {
  const Grid &B = *Fine_;
  for (int j = 0; j < B.H; j++) {
    for (int i = 0; i < B.W; i++) {
      const double e = B.OrgE + ((double)i + 0.5) * B.CellM;
      const double n = B.OrgN + ((double)j + 0.5) * B.CellM;
      Measures_.Probes++;
      if (Evaluate(e, n, nullptr, nullptr) < 0) { Measures_.NoData++; }
    }
  }
}

int ClassStructure::Evaluate(double e, double n, double *distM, int *runnerUp) const {
  int best = -1, bestRank = -1, second = -1, secondRank = -1;
  double bestDist = kNoEdgeM;
  const Grid *grids[2] = {Fine_.get(), Coarse_.get()};
  for (int b = 0; b < 2; b++) {
    const Grid &B = *grids[b];
    if (B.W == 0) { continue; }
    const int i = (int)std::floor((e - B.OrgE) / B.CellM);
    const int j = (int)std::floor((n - B.OrgN) / B.CellM);
    if (i < 0 || j < 0 || i >= B.W || j >= B.H) { continue; }
    const size_t ci = ((size_t)j * B.W + (size_t)i) * 2;
    const uint32_t c0 = B.Cells[ci];
    const uint32_t nseed = (c0 >> 16) & 0xFF;
    const uint32_t seedFirst = B.Cells[ci + 1];
    if ((c0 & 0xFF) != 0xFF) {
      best = (int)(c0 & 0xFF);
      bestRank = (int)((c0 >> 8) & 0xFF);
    }
    const double cx = B.OrgE + (double)i * B.CellM, cy = B.OrgN + (double)j * B.CellM;
    for (uint32_t s = 0; s < nseed; s++) {
      const uint32_t w0 = B.Seeds[(seedFirst + s) * 3];
      const uint32_t refFirst = B.Seeds[(seedFirst + s) * 3 + 1];
      float halfW;
      std::memcpy(&halfW, &B.Seeds[(seedFirst + s) * 3 + 2], sizeof halfW);
      const int tpl = (int)(w0 & 0xFF);
      const int rank = (int)((w0 >> 8) & 0xFF);
      const uint32_t nref = (w0 >> 16) & 0xFF;
      int wind = (int)((w0 >> 24) & 0xFF) - 128;
      double d = kNoEdgeM;
      for (uint32_t r = 0; r < nref; r++) {
        const float *p = &B.Edges[(size_t)B.Refs[refFirst + r] * 4];
        if (halfW <= 0.0f) {
          wind += CrossX(p[0], p[1], p[2], p[3], cy, cx, e);
          wind += CrossY(p[0], p[1], p[2], p[3], e, cy, n);
        }
        d = std::min(d, SegDist(e, n, p[0], p[1], p[2], p[3]));
      }
      if (halfW > 0.0f) {
        if (d > halfW) { continue; }
        d = (double)halfW - d;
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
  if (distM) { *distM = bestDist; }
  if (runnerUp) { *runnerUp = second; }
  return best;
}

} // namespace outshine

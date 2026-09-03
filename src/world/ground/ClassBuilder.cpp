#include "math/Units.h"
#include "math/Vec2.h"
#include "ClassBuilder.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>
#include <mutex>
#include <utility>
#include <optional>

#include "Capacity.h"
#include "StackProbe.h"

namespace outshine::Ground {

constexpr double kMsPerMicrosecond = 1e-3;

constexpr int kSignedByteLeast = -128;
constexpr int kSignedByteMost = 127;
constexpr int kSignedByteBias = 128;

namespace {

size_t GridBytes(const ClassStructure::Grid &g) {
  return CapacityBytes(g.Cells) + CapacityBytes(g.Seeds) + CapacityBytes(g.Refs) +
         CapacityBytes(g.Edges);
}

double Clock() {
  using namespace std::chrono;
  return static_cast<double>(
             duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) *
         kMsPerMicrosecond;
}

constexpr int kSeedCap = 32;
constexpr int kRefCap = 255;

constexpr float kCurveTolM = 0.60f;
constexpr uint8_t kFullCover = 0xFF;
constexpr unsigned kAlphaShift = 24u;
constexpr int kCurveMaxSplit = 8;

void CatmullPoint(
    const float *p0, const float *p1, const float *p2, const float *p3, float u, Vec2f &out) {
  const auto knot = [](float t, const float *a, const float *b) {
    const float dx = b[0] - a[0];
    const float dy = b[1] - a[1];
    return t + std::sqrt(std::sqrt(dx * dx + dy * dy) + static_cast<float>(kLeastRunM));
  };
  const float t0 = 0.0f;
  const float t1 = knot(t0, p0, p1);
  const float t2 = knot(t1, p1, p2);
  const float t3 = knot(t2, p2, p3);
  const float t = t1 + u * (t2 - t1);
  Vec2f a1;
  Vec2f a2;
  Vec2f a3;
  Vec2f b1;
  Vec2f b2;
  for (int a = 0; a < 2; a++) {
    a1[a] = ((t1 - t) * p0[a] + (t - t0) * p1[a]) / (t1 - t0);
    a2[a] = ((t2 - t) * p1[a] + (t - t1) * p2[a]) / (t2 - t1);
    a3[a] = ((t3 - t) * p2[a] + (t - t2) * p3[a]) / (t3 - t2);
  }
  for (int a = 0; a < 2; a++) {
    b1[a] = ((t2 - t) * a1[a] + (t - t0) * a2[a]) / (t2 - t0);
    b2[a] = ((t3 - t) * a2[a] + (t - t1) * a3[a]) / (t3 - t1);
  }
  for (int a = 0; a < 2; a++) { out[a] = ((t2 - t) * b1[a] + (t - t1) * b2[a]) / (t2 - t1); }
}

void CurveRing(
    const float *pts, uint32_t first, uint32_t count, bool closed, std::vector<float> &out) {
  out.clear();
  if (count < 2) { return; }
  const auto P = [&](int i) -> const float * {
    const int n = static_cast<int>(count);
    if (closed) {
      i = ((i % n) + n) % n;
    } else {
      i = std::clamp(i, 0, n - 1);
    }
    return pts + (static_cast<size_t>(first) + static_cast<size_t>(i)) * 2;
  };
  const int spans = closed ? static_cast<int>(count) : static_cast<int>(count) - 1;
  for (int s = 0; s < spans; s++) {
    const float *p0 = P(s - 1);
    const float *p1 = P(s);
    const float *p2 = P(s + 1);
    const float *p3 = P(s + 2);
    Vec2f mid;
    CatmullPoint(p0, p1, p2, p3, 0.5f, mid);
    const float cx = 0.5f * (p1[0] + p2[0]);
    const float cy = 0.5f * (p1[1] + p2[1]);
    const float dev = std::sqrt((mid[0] - cx) * (mid[0] - cx) + (mid[1] - cy) * (mid[1] - cy));
    int n = 1;
    if (dev > kCurveTolM) {
      n = static_cast<int>(std::ceil(std::sqrt(dev / kCurveTolM)));
      n = std::min(n, kCurveMaxSplit);
    }
    for (int k = 0; k < n; k++) {
      Vec2f q;
      CatmullPoint(p0, p1, p2, p3, static_cast<float>(k) / static_cast<float>(n), q);
      out.push_back(q[0]);
      out.push_back(q[1]);
    }
  }
  if (!closed) {
    const float *last = pts + (static_cast<size_t>(first) + count - 1) * 2;
    out.push_back(last[0]);
    out.push_back(last[1]);
  }
}

} // namespace

ClassBuilder::ClassBuilder()
    : Fine_(std::make_shared<const ClassStructure::Grid>()),
      Coarse_(std::make_shared<const ClassStructure::Grid>()),
      Thread_([this] { Run(); }) {}

ClassBuilder::~ClassBuilder() {
  {
    const std::scoped_lock lk(Mu_);
    Stop_ = true;
  }
  Cv_.notify_all();
  Thread_.join();
}

void ClassBuilder::Submit(Job job) {
  {
    const std::scoped_lock lk(Mu_);
    assert(Stage_ == Stage::Idle);
    Pending_ = std::move(job);
    Stage_ = Stage::Building;
  }
  Cv_.notify_one();
}

std::optional<ClassBuilder::Handback> ClassBuilder::Collect() {
  const std::scoped_lock lk(Mu_);
  if (Stage_ != Stage::Done) { return {}; }
  std::optional<Handback> out = std::move(Result_);
  Result_.reset();
  Stage_ = Stage::Idle;
  return out;
}

size_t ClassBuilder::ScratchBytes() const {
  const Workspace &w = Workspace_;
  return CapacityBytes(w.Base) + CapacityBytes(w.BaseRank) + CapacityBytes(w.SeedHead) +
         CapacityBytes(w.SeedNext) + CapacityBytes(w.SeedCount) + CapacityBytes(w.Edges) +
         CapacityBytes(w.Curve) + CapacityBytes(w.ByY) + CapacityBytes(w.Act) +
         CapacityBytes(w.CellHead) + CapacityBytes(w.CellNext) + CapacityBytes(w.CellStamp) +
         CapacityBytes(w.CellEdge) + CapacityBytes(w.CellCount) + CapacityBytes(w.Hits) +
         CapacityBytes(w.Seeds);
}

void ClassBuilder::Run() {
  StackProbe::Enter(StackProbe::Purpose::Class);
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lk(Mu_);
      Cv_.wait(lk, [this] { return Stop_ || Pending_.has_value(); });
      if (Stop_ || !Pending_) { return; }
      job = std::move(*Pending_);
      Pending_.reset();
    }
    auto grid = std::make_shared<ClassStructure::Grid>();
    int overflow = 0;
    const double t0 = Clock();
    LayDown(job, *grid, overflow);
    const double buildMs = Clock() - t0;

    if (job.Grain == ClassGrain::Fine) {
      Fine_ = std::move(grid);
    } else {
      Coarse_ = std::move(grid);
    }
    Version_++;
    Handback y;
    y.Structure = std::make_shared<const ClassStructure>(
        job.Frame,
        Fine_,
        Coarse_,
        ClassStructure::FromRun{.Version = Version_,
                                .UnmappedRow = job.UnmappedRow,
                                .BuildMs = buildMs,
                                .Overflow = overflow});
    y.Returned = std::move(job);
    HeapBytes_.store(GridBytes(*Fine_) + GridBytes(*Coarse_) + ScratchBytes(),
                     std::memory_order_relaxed);

    StackProbe::Mark();
    {
      const std::scoped_lock lk(Mu_);
      Result_ = std::move(y);
      Stage_ = Stage::Done;
    }
  }
}

void ClassBuilder::LayDown(const Job &job, ClassStructure::Grid &out, int &overflow) {
  Workspace &work = Workspace_;
  const int W = job.HalfCells * 2;
  const int H = job.HalfCells * 2;
  const double cell = job.CellM;
  out.W = W;
  out.H = H;
  out.CellM = cell;
  out.OrgE = std::floor(job.CamE / cell - job.HalfCells) * cell;
  out.OrgN = std::floor(job.CamN / cell - job.HalfCells) * cell;

  std::vector<uint8_t> &base = work.Base;
  std::vector<uint8_t> &baseRank = work.BaseRank;
  base.assign(static_cast<size_t>(W) * H, kFullCover);
  baseRank.assign(static_cast<size_t>(W) * H, 0);

  std::vector<int32_t> &seedHead = work.SeedHead;
  std::vector<int32_t> &seedNext = work.SeedNext;
  std::vector<uint32_t> &seedCount = work.SeedCount;
  seedHead.assign(static_cast<size_t>(W) * H, -1);
  seedNext.clear();
  seedCount.assign(static_cast<size_t>(W) * H, 0);

  std::vector<float> &ex = work.Edges;
  std::vector<float> &curve = work.Curve;
  std::vector<uint32_t> &byY = work.ByY;
  std::vector<uint32_t> &act = work.Act;

  std::vector<int32_t> &ceHead = work.CellHead;
  std::vector<int32_t> &ceNext = work.CellNext;
  std::vector<uint32_t> &ceStamp = work.CellStamp;
  std::vector<uint32_t> &ceEdge = work.CellEdge;
  std::vector<uint32_t> &ceCount = work.CellCount;
  std::ranges::fill(ceStamp, 0u);
  uint32_t stamp = 0;
  std::vector<Hit> &hits = work.Hits;

  for (const Feature &f : job.Feats) {
    if (f.MaxE < out.OrgE || f.MinE > out.OrgE + W * cell) { continue; }
    if (f.MaxN < out.OrgN || f.MinN > out.OrgN + H * cell) { continue; }

    ex.clear();
    if (f.Form == Shape::Polygon) {
      for (uint32_t k = 0; k < f.RingCount; k++) {
        const Ring &ring = job.Rings[f.FirstRing + k];
        CurveRing(job.Pts.data(), ring.First, ring.Count, true, curve);
        const size_t nc = curve.size() / 2;
        for (size_t s = 0; s < nc; s++) {
          const size_t a = s;
          const size_t b = (s + 1) % nc;
          ex.push_back(curve[a * 2]);
          ex.push_back(curve[a * 2 + 1]);
          ex.push_back(curve[b * 2]);
          ex.push_back(curve[b * 2 + 1]);
        }
      }
    } else {
      for (uint32_t k = 0; k < f.RingCount; k++) {
        const Ring &ring = job.Rings[f.FirstRing + k];
        CurveRing(job.Pts.data(), ring.First, ring.Count, false, curve);
        const size_t nc = curve.size() / 2;
        for (size_t i = 0; i + 1 < nc; i++) {
          ex.push_back(curve[i * 2]);
          ex.push_back(curve[i * 2 + 1]);
          ex.push_back(curve[(i + 1) * 2]);
          ex.push_back(curve[(i + 1) * 2 + 1]);
        }
      }
    }
    const size_t ne = ex.size() / 4;
    if (ne == 0) { continue; }

    const int i0 = std::max(0, static_cast<int>(std::floor((f.MinE - out.OrgE) / cell)));
    const int i1 = std::min(W - 1, static_cast<int>(std::floor((f.MaxE - out.OrgE) / cell)));
    const int j0 = std::max(0, static_cast<int>(std::floor((f.MinN - out.OrgN) / cell)));
    const int j1 = std::min(H - 1, static_cast<int>(std::floor((f.MaxN - out.OrgN) / cell)));
    if (i0 > i1 || j0 > j1) { continue; }
    const int bw = i1 - i0 + 1;
    const int bh = j1 - j0 + 1;

    stamp++;
    if (ceHead.size() < static_cast<size_t>(bw) * bh) {
      ceHead.resize(static_cast<size_t>(bw) * bh, -1);
      ceStamp.resize(static_cast<size_t>(bw) * bh, 0);
      ceCount.resize(static_cast<size_t>(bw) * bh, 0);
    }
    ceNext.clear();
    ceEdge.clear();

    const float epad = (f.Form == Shape::Polygon) ? 0.0f : f.WidthM * 0.5f;
    for (size_t e = 0; e < ne; e++) {
      const float *p = &ex[e * 4];
      const int ei0 = std::max(
          i0, static_cast<int>(std::floor((std::min(p[0], p[2]) - epad - out.OrgE) / cell)));
      const int ei1 = std::min(
          i1, static_cast<int>(std::floor((std::max(p[0], p[2]) + epad - out.OrgE) / cell)));
      const int ej0 = std::max(
          j0, static_cast<int>(std::floor((std::min(p[1], p[3]) - epad - out.OrgN) / cell)));
      const int ej1 = std::min(
          j1, static_cast<int>(std::floor((std::max(p[1], p[3]) + epad - out.OrgN) / cell)));
      for (int j = ej0; j <= ej1; j++) {
        for (int i = ei0; i <= ei1; i++) {
          const size_t c = static_cast<size_t>(j - j0) * bw + static_cast<size_t>(i - i0);
          if (ceStamp[c] != stamp) {
            ceStamp[c] = stamp;
            ceHead[c] = -1;
            ceCount[c] = 0;
          }
          ceNext.push_back(ceHead[c]);
          ceEdge.push_back(static_cast<uint32_t>(e));
          ceHead[c] = static_cast<int32_t>(ceNext.size() - 1);
          ceCount[c]++;
        }
      }
    }

    byY.resize(ne);
    for (uint32_t e = 0; e < static_cast<uint32_t>(ne); e++) { byY[e] = e; }
    std::ranges::sort(byY, [&ex](uint32_t a, uint32_t b) {
      return std::min(ex[static_cast<size_t>(a) * 4 + 1], ex[static_cast<size_t>(a) * 4 + 3]) <
             std::min(ex[static_cast<size_t>(b) * 4 + 1], ex[static_cast<size_t>(b) * 4 + 3]);
    });
    act.clear();
    size_t nextE = 0;

    for (int j = j0; j <= j1; j++) {
      const double cy = out.OrgN + static_cast<double>(j) * cell;
      while (nextE < ne) {
        const float *p = &ex[static_cast<size_t>(byY[nextE]) * 4];
        if (static_cast<double>(std::min(p[1], p[3])) > cy) { break; }
        act.push_back(byY[nextE]);
        nextE++;
      }
      size_t keep = 0;
      for (size_t k = 0; k < act.size(); k++) {
        const float *p = &ex[static_cast<size_t>(act[k]) * 4];
        if (static_cast<double>(std::max(p[1], p[3])) > cy) { act[keep++] = act[k]; }
      }
      act.resize(keep);

      hits.clear();
      for (const uint32_t e : act) {
        const float *p = &ex[static_cast<size_t>(e) * 4];
        if ((p[1] <= cy) == (p[3] <= cy)) { continue; }
        const double xi = static_cast<double>(p[0]) +
                          (cy - static_cast<double>(p[1])) *
                              (static_cast<double>(p[2]) - static_cast<double>(p[0])) /
                              (static_cast<double>(p[3]) - static_cast<double>(p[1]));
        hits.push_back(Hit{.X = xi, .Dir = p[3] > p[1] ? 1 : -1});
      }
      std::ranges::sort(hits, [](const Hit &a, const Hit &b) { return a.X < b.X; });

      int wind = 0;
      size_t hi = 0;
      for (int i = i0; i <= i1; i++) {
        const double cx = out.OrgE + static_cast<double>(i) * cell;
        while (hi < hits.size() && hits[hi].X < cx) {
          wind += hits[hi].Dir;
          hi++;
        }
        const size_t bc = static_cast<size_t>(j - j0) * bw + static_cast<size_t>(i - i0);
        const uint32_t nce = ceStamp[bc] == stamp ? ceCount[bc] : 0u;
        const size_t ci = static_cast<size_t>(j) * W + static_cast<size_t>(i);
        if (nce == 0 || seedCount[ci] >= static_cast<uint32_t>(kSeedCap) ||
            nce > static_cast<uint32_t>(kRefCap)) {
          if (nce != 0) { overflow++; }

          if (f.Form == Shape::Polygon && wind != 0) {
            base[ci] = static_cast<uint8_t>(f.Tpl);
            baseRank[ci] = static_cast<uint8_t>(f.Rank);
          }
          continue;
        }
        const auto refFirst = static_cast<uint32_t>(out.Refs.size());
        for (int32_t k = ceHead[bc]; k >= 0; k = ceNext[static_cast<size_t>(k)]) {
          out.Refs.push_back(static_cast<uint32_t>(out.Edges.size() / 4) +
                             ceEdge[static_cast<size_t>(k)]);
        }
        seedNext.push_back(seedHead[ci]);
        seedHead[ci] = static_cast<int32_t>(out.Seeds.size() / 3);
        seedCount[ci]++;
        out.Seeds.push_back(
            static_cast<uint32_t>(f.Tpl) | (static_cast<uint32_t>(f.Rank) << 8u) | (nce << 16u) |
            (static_cast<uint32_t>(static_cast<uint8_t>(
                 std::max(kSignedByteLeast, std::min(kSignedByteMost, wind)) + kSignedByteBias))
             << kAlphaShift));
        out.Seeds.push_back(refFirst);
        {
          const float hw = (f.Form == Shape::Polygon) ? 0.0f : f.WidthM * 0.5f;
          uint32_t bits;
          std::memcpy(&bits, &hw, sizeof bits);
          out.Seeds.push_back(bits);
        }
      }
    }
    out.Edges.insert(out.Edges.end(), ex.begin(), ex.end());
  }

  std::vector<uint32_t> &seeds = work.Seeds;
  seeds.clear();
  seeds.reserve(out.Seeds.size());
  out.Cells.assign(static_cast<size_t>(W) * H * 2, 0);
  for (size_t ci = 0; ci < static_cast<size_t>(W) * H; ci++) {
    const auto first = static_cast<uint32_t>(seeds.size() / 3);
    for (int32_t s = seedHead[ci]; s >= 0; s = seedNext[static_cast<size_t>(s)]) {
      seeds.push_back(out.Seeds[static_cast<size_t>(s) * 3]);
      seeds.push_back(out.Seeds[static_cast<size_t>(s) * 3 + 1]);
      seeds.push_back(out.Seeds[static_cast<size_t>(s) * 3 + 2]);
    }
    out.Cells[ci * 2] = static_cast<uint32_t>(base[ci]) |
                        (static_cast<uint32_t>(baseRank[ci]) << 8u) |
                        (static_cast<uint32_t>(seedCount[ci]) << 16u);
    out.Cells[ci * 2 + 1] = first;
  }
  out.Seeds.swap(seeds);
}

} // namespace outshine::Ground

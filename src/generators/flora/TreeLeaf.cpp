#include "math/Units.h"
#include "TreeFrame.h"
#include "TreeLeaf.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <numbers>
#include <cmath>
#include <vector>
#include <span>

#include "TreeRandom.h"

namespace outshine::Generators {

constexpr float kBaseFillFalloff = 9.0f;

constexpr float kTipGain = 1.25f;

namespace {

constexpr float kNeedleNarrowing = 0.12f;
constexpr float kNeedleTipFrom = 0.82f;
constexpr float kNeedleTipSpan = 0.18f;
constexpr float kBroadWidestGain = 1.5f;
constexpr float kBroadBase = 0.95f;
constexpr float kBroadLeast = 0.55f;
constexpr float kSerrationBite = 0.45f;

constexpr float kQuarterTurnRad = std::numbers::pi_v<float> / 2.0f;
constexpr float kOutlineFloor = 0.82f;
constexpr float kOutlineSwing = 0.18f;
constexpr float kSerrationDepth = 0.10f;
constexpr float kSegmentM = 0.0085f;
constexpr int kSegmentsLeast = 44;
constexpr int kSegmentsMost = 180;
constexpr uint32_t kBladeSeed = 99u;
constexpr float kBladeTipShare = 0.93f;

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
constexpr float kDeg = static_cast<float>(kDeg2Rad);

class Sink {
public:
  explicit Sink(TreeMesh &m, float maxDeviation, float maxRelativeAreaError)
      : Mesh_(m), MaxDeviation_(maxDeviation), MaxRelativeAreaError_(maxRelativeAreaError) {}

  [[nodiscard]] float MaxDeviation() const { return MaxDeviation_; }

  [[nodiscard]] float MaxRelativeAreaError() const { return MaxRelativeAreaError_; }

  uint32_t Vert(Vec3f p, Vec3f n, float u, float v) {
    const auto idx = static_cast<uint32_t>(Mesh_.LeafVerts.size() / TreeMesh::kLeafFloats);
    Mesh_.LeafVerts.insert(Mesh_.LeafVerts.end(), {p[0], p[1], p[2], n[0], n[1], n[2], u, v});
    return idx;
  }

  void Tri(uint32_t a, uint32_t b, uint32_t c) {
    Mesh_.LeafIdx.push_back(a);
    Mesh_.LeafIdx.push_back(b);
    Mesh_.LeafIdx.push_back(c);
  }

private:
  TreeMesh &Mesh_;
  float MaxDeviation_, MaxRelativeAreaError_;
};

float ProfileWidth(const TreeSpecies::Leaf &p, float t) {
  if (p.Kind == TreeSpecies::LeafKind::Needle) {
    float w = p.NeedleWidth * (1.0f - kNeedleNarrowing * t);
    if (t > kNeedleTipFrom) { w *= (1.0f - t) / kNeedleTipSpan; }
    return w;
  }
  const float a = p.Widest * 1.5f + 0.80f;
  float b = (1.0f - p.Widest) * kBroadWidestGain + kBroadBase - p.Tip * kTipGain;
  b = std::max(b, kBroadLeast);
  const float peak = std::pow(p.Widest, a) * std::pow(1.0f - p.Widest, b);
  float w = (peak > static_cast<float>(kLeastRunM)) ? std::pow(t, a) * std::pow(1.0f - t, b) / peak
                                                    : 0.0f;
  w *= p.Width;
  if (p.BaseFill > 0.0f) { w += p.BaseFill * p.Width * std::exp(-t * kBaseFillFalloff); }
  if (p.Lobes > 0) {
    const float lob = 0.5f + 0.5f * std::cos(kTau * static_cast<float>(p.Lobes) * t);
    w *= 1.0f - p.LobeDepth * lob;
  }
  if (p.Serration > 0.0f) {
    const float f = static_cast<float>(p.Lobes > 0 ? p.Lobes : 7) * 2.0f;
    const float saw = std::fabs(2.0f * (t * f - std::floor(t * f + 0.5f)));
    w *= 1.0f - p.Serration * kSerrationBite * saw;
  }
  return w;
}

struct Blade {
  float AngleRad = 0.0f;
  float LengthScale = 1.0f;
};

double StripArea(std::span<const Vec3f> positions, size_t first, size_t last) {
  double area = 0.0;
  for (size_t side = 0; side < 2; ++side) {
    const Vec3f a = positions[first * 3 + side], b = positions[first * 3 + side + 1];
    const Vec3f c = positions[last * 3 + side + 1], d = positions[last * 3 + side];
    area += 0.5 * (static_cast<double>(Length(Cross(b - a, c - a))) +
                   static_cast<double>(Length(Cross(c - a, d - a))));
  }
  return area;
}

float StripError(std::span<const Vec3f> positions, size_t first, size_t last) {
  float error = 0.0f;
  for (size_t row = first; row <= last; ++row) {
    const float t = static_cast<float>(row - first) / static_cast<float>(last - first);
    for (size_t side = 0; side < 3; ++side) {
      const Vec3f chord = positions[first * 3 + side] * (1.0f - t) + positions[last * 3 + side] * t;
      error = std::max(error, Length(positions[row * 3 + side] - chord));
    }
    for (size_t side = 0; side < 2; ++side) {
      const Vec3f fine = positions[row * 3 + side] * (1.0f - t) + positions[row * 3 + side + 1] * t;
      const Vec3f coarse =
          positions[first * 3 + side] * (1.0f - t) + positions[last * 3 + side + 1] * t;
      error = std::max(error, Length(fine - coarse));
    }
    if (row == last) { continue; }
    const float crossing = static_cast<float>(row - first) / static_cast<float>(last - first - 1);
    for (size_t side = 0; side < 2; ++side) {
      const Vec3f fine = positions[row * 3 + side] * (1.0f - crossing) +
                         positions[(row + 1) * 3 + side + 1] * crossing;
      const Vec3f coarse = positions[first * 3 + side] * (1.0f - crossing) +
                           positions[last * 3 + side + 1] * crossing;
      error = std::max(error, Length(fine - coarse));
    }
  }
  return error;
}

void BuildBlade(Sink &sink, const TreeSpecies::Leaf &p, Vec3f base, Blade held) {
  const float ang = held.AngleRad;
  const float lenScale = held.LengthScale;
  int n = p.Segments;
  n = std::max(n, 4);
  const int nv = (n + 1) * 3;
  std::vector<Vec3f> pos(static_cast<size_t>(nv));
  std::vector<Vec3f> nrm(static_cast<size_t>(nv));
  std::vector<float> uu(static_cast<size_t>(nv));
  const Vec3f dir = Vec3f{{std::sin(ang), std::cos(ang), 0.0f}};
  const Vec3f side = Vec3f{{std::cos(ang), -std::sin(ang), 0.0f}};
  const float len = p.Length * lenScale;
  for (int i = 0; i <= n; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(n);
    const float w = ProfileWidth(p, t) * len;
    const float z = -p.Curve * len * t * t;
    const float fz = p.Fold * w;
    const float skf = p.BaseSkew * (1.0f - t);
    const float wl = w * (1.0f + skf * 0.9f);
    const float wr = w * (1.0f - skf * 0.9f);
    const Vec3f c = base + (dir * (t * len) + Vec3f{{0, 0, z}});
    const int b3 = i * 3;
    pos[static_cast<size_t>(b3) + 0] = c + (side * (-wl) + Vec3f{{0, 0, fz}});
    uu[static_cast<size_t>(b3) + 0] = 0.0f;
    pos[static_cast<size_t>(b3) + 1] = c;
    uu[static_cast<size_t>(b3) + 1] = 0.5f;
    pos[static_cast<size_t>(b3) + 2] = c + (side * wr + Vec3f{{0, 0, fz}});
    uu[static_cast<size_t>(b3) + 2] = 1.0f;
  }
  for (int i = 0; i < n; ++i) {
    const int a = i * 3;
    const int d = (i + 1) * 3;
    const std::array<std::array<int, 3>, 4> tri = {
        {{a, a + 1, d + 1}, {a, d + 1, d}, {a + 1, a + 2, d + 2}, {a + 1, d + 2, d + 1}}};
    for (auto k : tri) {
      const Vec3f fn = Cross(pos[static_cast<size_t>(k[1])] - pos[static_cast<size_t>(k[0])],
                             pos[static_cast<size_t>(k[2])] - pos[static_cast<size_t>(k[0])]);
      for (int e = 0; e < 3; ++e) {
        nrm[static_cast<size_t>(k[e])] = nrm[static_cast<size_t>(k[e])] + fn;
      }
    }
  }
  std::vector<double> areas(static_cast<size_t>(n) + 1, 0.0);
  for (size_t row = 0; row < static_cast<size_t>(n); ++row) {
    areas[row + 1] = areas[row] + StripArea(pos, row, row + 1);
  }
  const double areaBudget = areas.back() * sink.MaxRelativeAreaError() / static_cast<double>(n);
  std::vector<size_t> rows{0};
  for (size_t first = 0; first < static_cast<size_t>(n);) {
    size_t last = first + 1;
    if (sink.MaxDeviation() > 0.0f) {
      for (size_t candidate = first + 2; candidate <= static_cast<size_t>(n); ++candidate) {
        if (StripError(pos, first, candidate) <= sink.MaxDeviation() &&
            std::abs(StripArea(pos, first, candidate) - (areas[candidate] - areas[first])) <=
                areaBudget * static_cast<double>(candidate - first)) {
          last = candidate;
        }
      }
    }
    rows.push_back(last);
    first = last;
  }
  std::vector<uint32_t> idx;
  idx.reserve(rows.size() * 3);
  for (const size_t row : rows) {
    for (size_t sideIndex = 0; sideIndex < 3; ++sideIndex) {
      const size_t at = row * 3 + sideIndex;
      idx.push_back(sink.Vert(pos[at],
                              DirectionOrUp(nrm[at]),
                              uu[at],
                              static_cast<float>(row) / static_cast<float>(n)));
    }
  }
  for (size_t row = 0; row + 1 < rows.size(); ++row) {
    const size_t a = row * 3, d = a + 3;
    sink.Tri(idx[a], idx[a + 1], idx[d + 1]);
    sink.Tri(idx[a], idx[d + 1], idx[d]);
    sink.Tri(idx[a + 1], idx[a + 2], idx[d + 2]);
    sink.Tri(idx[a + 1], idx[d + 2], idx[d + 1]);
  }
}

void BuildPalmate(Sink &sink, const TreeSpecies::Leaf &p) {
  const int r = 6;
  int a = p.Segments;
  a = std::max(a, 16);
  int nl = p.PalmateLobes;
  nl = std::max(nl, 3);
  const float spread = p.PalmateSpread * kDeg;

  const int nv = 1 + r * (a + 1);
  std::vector<Vec3f> pos(static_cast<size_t>(nv));
  std::vector<Vec3f> nrm(static_cast<size_t>(nv));
  std::vector<float> uu(static_cast<size_t>(nv));
  std::vector<float> vv(static_cast<size_t>(nv));
  pos[0] = Vec3f{{0, 0, 0}};
  uu[0] = 0.5f;
  vv[0] = 0.0f;

  for (int i = 1; i <= r; ++i) {
    for (int j = 0; j <= a; ++j) {
      const float t = static_cast<float>(j) / static_cast<float>(a);
      const float th = -spread + 2.0f * spread * t;
      const float x = t * static_cast<float>(nl - 1);
      const float fk = x - std::floor(x);
      const float tri = std::fabs(2.0f * fk - 1.0f);
      float outl = p.Length * (1.0f - p.LobeDepth * (1.0f - tri));
      outl *= kOutlineFloor + kOutlineSwing * std::cos(th * (kQuarterTurnRad / spread));
      if (p.Serration > 0.0f) {
        const float f = static_cast<float>(nl - 1) * 4.0f;
        const float saw = std::fabs(2.0f * (t * f - std::floor(t * f + 0.5f)));
        outl *= 1.0f - p.Serration * kSerrationDepth * saw;
      }
      const float rr = outl * static_cast<float>(i) / static_cast<float>(r);
      const Vec3f dir = Vec3f{{std::sin(th), std::cos(th), 0.0f}};
      const float z = -p.Curve * p.Length * static_cast<float>(i * i) / static_cast<float>(r * r);
      const int idx = 1 + (i - 1) * (a + 1) + j;
      pos[static_cast<size_t>(idx)] = dir * rr + Vec3f{{0, 0, z}};
      uu[static_cast<size_t>(idx)] = t;
      vv[static_cast<size_t>(idx)] = static_cast<float>(i) / static_cast<float>(r);
    }
  }
  const auto at = [a](int i, int j) {
    return 1u + static_cast<size_t>(i - 1) * static_cast<size_t>(a + 1) + static_cast<size_t>(j);
  };
  for (int j = 0; j < a; ++j) {
    const size_t b = at(1, j);
    const size_t c = at(1, j + 1);
    const Vec3f fn = Cross(pos[b] - pos[0], pos[c] - pos[0]);
    nrm[0] = nrm[0] + fn;
    nrm[b] = nrm[b] + fn;
    nrm[c] = nrm[c] + fn;
  }
  for (int i = 1; i < r; ++i) {
    for (int j = 0; j < a; ++j) {
      const std::array<size_t, 4> q = {{at(i, j), at(i, j + 1), at(i + 1, j + 1), at(i + 1, j)}};
      const std::array<std::array<size_t, 3>, 2> tr = {{{q[0], q[1], q[2]}, {q[0], q[2], q[3]}}};
      for (const auto &k : tr) {
        const Vec3f fn = Cross(pos[k[1]] - pos[k[0]], pos[k[2]] - pos[k[0]]);
        for (int e = 0; e < 3; ++e) { nrm[k[e]] = nrm[k[e]] + fn; }
      }
    }
  }
  std::vector<uint32_t> idx(static_cast<size_t>(nv));
  for (int i = 0; i < nv; ++i) {
    idx[static_cast<size_t>(i)] = sink.Vert(pos[static_cast<size_t>(i)],
                                            DirectionOrUp(nrm[static_cast<size_t>(i)]),
                                            uu[static_cast<size_t>(i)],
                                            vv[static_cast<size_t>(i)]);
  }
  for (int j = 0; j < a; ++j) { sink.Tri(idx[0], idx[at(1, j)], idx[at(1, j + 1)]); }
  for (int i = 1; i < r; ++i) {
    for (int j = 0; j < a; ++j) {
      sink.Tri(idx[at(i, j)], idx[at(i, j + 1)], idx[at(i + 1, j + 1)]);
      sink.Tri(idx[at(i, j)], idx[at(i + 1, j + 1)], idx[at(i + 1, j)]);
    }
  }
}

void BuildNeedleShoot(Sink &sink, const TreeSpecies::Leaf &p) {
  const float len = p.Length;
  int n = static_cast<int>(len / kSegmentM);
  n = std::max(n, kSegmentsLeast);
  n = std::min(n, kSegmentsMost);
  const float nl = len * p.NeedleLen;
  const float nw = std::fmax(p.NeedleWidth * 0.26f, 0.0042f);
  const float fwd = p.NeedleFwd;
  TreeRandom rng(kBladeSeed);
  const float sw = len * 0.010f;
  const Vec3f up = Vec3f{{0, 0, 1}};
  const uint32_t s0 = sink.Vert(Vec3f{{-sw, 0, 0}}, up, 0.47f, 0.0f);
  const uint32_t s1 = sink.Vert(Vec3f{{sw, 0, 0}}, up, 0.53f, 0.0f);
  const uint32_t s2 = sink.Vert(Vec3f{{sw, len, 0}}, up, 0.53f, 1.0f);
  const uint32_t s3 = sink.Vert(Vec3f{{-sw, len, 0}}, up, 0.47f, 1.0f);
  sink.Tri(s0, s1, s2);
  sink.Tri(s0, s2, s3);
  for (int i = 0; i < n; ++i) {
    const float t = static_cast<float>(i + 1) / static_cast<float>(n + 1);
    const float y = t * len;
    const int side = ((i % 2) != 0) ? 1 : -1;
    const float sx = static_cast<float>(side) * (1.0f - fwd) + rng.Signed() * 0.05f;
    const Vec3f dir = DirectionOrUp(Vec3f{{sx, fwd + rng.Signed() * 0.10f, 0.0f}});
    const Vec3f base = Vec3f{{0, y, 0}};
    const Vec3f tip = base + dir * (nl * (0.8f + 0.35f * rng.Unit()));
    const Vec3f perp = DirectionOrUp(Vec3f{{dir[1], -dir[0], 0.0f}}) * nw;
    const uint32_t a = sink.Vert(base - perp, up, 0.0f, t);
    const uint32_t b = sink.Vert(base + perp, up, 1.0f, t);
    const uint32_t c = sink.Vert(tip, up, 0.5f, t);
    sink.Tri(a, b, c);
  }
}

void BuildPinnate(Sink &sink, const TreeSpecies::Leaf &p) {
  const int pairs = p.Leaflets > 0 ? p.Leaflets : 5;
  const float len = p.Length;
  const float rw = len * 0.006f;
  const Vec3f up = Vec3f{{0, 0, 1}};
  const uint32_t r0 = sink.Vert(Vec3f{{-rw, 0, 0}}, up, 0.48f, 0.0f);
  const uint32_t r1 = sink.Vert(Vec3f{{rw, 0, 0}}, up, 0.52f, 0.0f);
  const uint32_t r2 = sink.Vert(Vec3f{{rw, len * 0.95f, 0}}, up, 0.52f, 1.0f);
  const uint32_t r3 = sink.Vert(Vec3f{{-rw, len * 0.95f, 0}}, up, 0.48f, 1.0f);
  sink.Tri(r0, r1, r2);
  sink.Tri(r0, r2, r3);
  const float ls = 0.34f;
  const float ang = 1.02f;
  for (int i = 0; i < pairs; ++i) {
    const float t =
        0.14f + 0.78f * (pairs > 1 ? static_cast<float>(i) / static_cast<float>(pairs - 1) : 0.5f);
    const Vec3f base = Vec3f{{0, t * len, 0}};
    BuildBlade(sink, p, base, {.AngleRad = ang, .LengthScale = ls});
    BuildBlade(sink, p, base, {.AngleRad = -ang, .LengthScale = ls});
  }
  BuildBlade(sink, p, Vec3f{{0, len * kBladeTipShare, 0}}, {.AngleRad = 0.0f, .LengthScale = ls});
}

void BuildPalmateCompound(Sink &sink, const TreeSpecies::Leaf &p) {
  const int n = p.Leaflets > 0 ? p.Leaflets : 5;
  const float spread = (p.PalmateSpread > 0 ? p.PalmateSpread : 80.0f) * kDeg;
  for (int i = 0; i < n; ++i) {
    const float t = (n > 1) ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.5f;
    const float ang = -spread + 2.0f * spread * t;
    const float ls = 0.62f + 0.38f * (1.0f - std::fabs(2.0f * t - 1.0f));
    BuildBlade(sink, p, Vec3f{{0, 0, 0}}, {.AngleRad = ang, .LengthScale = ls});
  }
}

}

void TreeLeaf::Build(const TreeSpecies::Leaf &leaf,
                     TreeMesh &out,
                     float maxDeviation,
                     float maxRelativeAreaError) {
  out.LeafVerts.clear();
  out.LeafIdx.clear();
  Sink sink(out, maxDeviation, maxRelativeAreaError);
  switch (leaf.Kind) {
    case TreeSpecies::LeafKind::Palmate: BuildPalmate(sink, leaf); break;
    case TreeSpecies::LeafKind::Pinnate: BuildPinnate(sink, leaf); break;
    case TreeSpecies::LeafKind::PalmateCompound: BuildPalmateCompound(sink, leaf); break;
    case TreeSpecies::LeafKind::Needle: BuildNeedleShoot(sink, leaf); break;
    case TreeSpecies::LeafKind::Broad:
      BuildBlade(sink, leaf, Vec3f{{0, 0, 0}}, {.AngleRad = 0.0f, .LengthScale = 1.0f});
      break;
  }
}

}

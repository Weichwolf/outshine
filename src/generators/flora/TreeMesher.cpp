#include <span>
#include "TreeFrame.h"
#include "TreeMesher.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <cmath>

#include "TreeRandom.h"

namespace outshine::Generators {

constexpr float kCapRise = 1.4f;

constexpr float kChordShare = 0.9f;
constexpr float kBaseApex = -0.6f;
constexpr float kPointApex = 2.4f;
constexpr float kBrokenApex = 1.4f;
constexpr uint32_t kKnuthWord = 2654435761u;

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;

Vec3f RingDir(const TreeSkeleton::Node &node, float angle) {
  const Vec3f b = DirectionOrUp(Cross(node.Dir, node.Up));
  return node.Up * std::cos(angle) + b * std::sin(angle);
}

} // namespace

int TreeMesher::AddVert(Vec3f p) {
  Verts_.push_back(p);
  return static_cast<int>(Verts_.size()) - 1;
}

int TreeMesher::AddFace(int a, int b, int c, int d) {
  Faces_.push_back(Face{.A = a, .B = b, .C = c, .D = d});
  Dead_.push_back(0);
  return static_cast<int>(Faces_.size()) - 1;
}

Vec3f TreeMesher::FaceCentroid(int fi) const {
  const Face &f = Faces_[static_cast<size_t>(fi)];
  const int n = f.D < 0 ? 3 : 4;
  const std::array<int, 4> idx = {{f.A, f.B, f.C, f.D}};
  Vec3f s;
  for (int i = 0; i < n; ++i) { s = s + Verts_[static_cast<size_t>(idx[i])]; }
  return s * (1.0f / static_cast<float>(n));
}

int TreeMesher::SidesFor(Sided of) const {
  const float radius = of.RadiusM;
  const int declared = of.Declared;
  int cap = declared < kMaxSides ? declared : kMaxSides;
  cap = std::max(cap, 3);
  if (PixelGrow_ <= 0.0f || radius <= 0.0f) { return cap; }
  const float c = 1.0f - 0.5f * PixelGrow_ / radius;
  if (c <= -1.0f) { return 3; }
  const int n = static_cast<int>(std::ceil(std::numbers::pi_v<float> / std::acos(c)));
  if (n < 3) { return 3; }
  return n < cap ? n : cap;
}

bool TreeMesher::ChordHolds(const TreeSkeleton &plant, int from, int last, int stride) const {
  const float tol = 0.5f * PixelGrow_;
  for (int b = last; b - stride >= from; b -= stride) {
    const int a = b - stride;
    const TreeSkeleton::Node &na = plant.Nodes[static_cast<size_t>(a)];
    const TreeSkeleton::Node &nb = plant.Nodes[static_cast<size_t>(b)];
    const Vec3f chord = nb.Pos - na.Pos;
    const float span = Dot(chord, chord);
    if (span <= 0.0f) { continue; }
    for (int i = a + 1; i < b; ++i) {
      const TreeSkeleton::Node &n = plant.Nodes[static_cast<size_t>(i)];
      const float t = Dot(n.Pos - na.Pos, chord) / span;
      const Vec3f off = n.Pos - (na.Pos + chord * t);

      if (Length(off) + std::fabs(n.Radius - (na.Radius + (nb.Radius - na.Radius) * t)) > tol) {
        return false;
      }
    }
  }
  return true;
}

void TreeMesher::RingsOf(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot, int from) {
  Stations_.clear();
  const int last = shoot.First + shoot.Count - 1;
  int stride = 1;
  if (PixelGrow_ > 0.0f) {
    for (uint32_t trial = 2; from + static_cast<int>(trial) <= last &&
                             ChordHolds(plant, from, last, static_cast<int>(trial));
         trial <<= 1u) {
      stride = static_cast<int>(trial);
    }
  }
  for (int i = last; i > from; i -= stride) { Stations_.push_back(i); }
  Stations_.push_back(from);
  std::ranges::reverse(Stations_);
}

float TreeMesher::RoomAt(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot) {
  const TreeSkeleton::Node &upper = plant.Nodes[static_cast<size_t>(shoot.ParentNode)];
  const TreeSkeleton::Node &lower = plant.Nodes[static_cast<size_t>(shoot.ParentNode - 1)];
  int sides = plant.Shoots[static_cast<size_t>(shoot.Parent)].Sides;
  sides = std::min(sides, kMaxSides);
  sides = std::max(sides, 3);
  const float along = 0.5f * Length(upper.Pos - lower.Pos);
  const float around = upper.Radius * std::sin(kTau * 0.5f / static_cast<float>(sides));
  return kChordShare * std::sqrt(along * along + around * around);
}

void TreeMesher::Ring(const TreeSkeleton::Node &node, float radius, int sides, std::span<int> out) {
  for (int j = 0; j < sides; ++j) {
    out[j] =
        AddVert(node.Pos +
                RingDir(node, kTau * static_cast<float>(j) / static_cast<float>(sides)) * radius);
  }
}

void TreeMesher::Wall(std::span<const int> from, std::span<const int> to, int sides) {
  for (int j = 0; j < sides; ++j) {
    AddFace(from[j], from[(j + 1) % sides], to[(j + 1) % sides], to[j]);
  }
}

void TreeMesher::BreakProfile(Splintered of, std::span<float> out) {
  const uint32_t seed = of.Seed;
  const int sides = of.Sides;
  std::array<float, kMaxSides> splinter{};
  TreeRandom rng(seed);
  for (float &i : splinter) { i = rng.Unit(); }
  for (int j = 0; j < sides; ++j) {
    const float at = kTau * static_cast<float>(j) / static_cast<float>(sides);
    const float arc = kTau * 0.5f / static_cast<float>(sides);
    float lowest = 1.0f;
    for (int i = 0; i < kMaxSides; ++i) {
      float apart = std::fabs(kTau * static_cast<float>(i) / static_cast<float>(kMaxSides) - at);
      if (apart > kTau * 0.5f) { apart = kTau - apart; }
      if (apart <= arc && splinter[i] < lowest) { lowest = splinter[i]; }
    }
    out[j] = lowest;
  }
}

void TreeMesher::Cap(const TreeSkeleton::Node &node,
                     std::span<const int> ring,
                     int sides,
                     RingCap cap,
                     uint32_t seed) {
  float apex = 0.0f;
  switch (cap) {
    case RingCap::Base: apex = kBaseApex; break;
    case RingCap::Point: apex = kPointApex; break;
    case RingCap::Cut: apex = 0.0f; break;

    case RingCap::Broken: apex = kBrokenApex; break;
  }
  const int ci = AddVert(node.Pos + node.Dir * (node.Radius * apex));
  std::array<float, kMaxSides> splinter = {{}};
  if (cap == RingCap::Broken) { BreakProfile({.Seed = seed, .Sides = sides}, splinter); }
  for (int j = 0; j < sides; ++j) {
    if (cap == RingCap::Broken) {
      Verts_[static_cast<size_t>(ring[j])] =
          Verts_[static_cast<size_t>(ring[j])] + node.Dir * (node.Radius * splinter[j] * kCapRise);
    }
    if (cap == RingCap::Base) {
      AddFace(ci, ring[(j + 1) % sides], ring[j], -1);
    } else {
      AddFace(ci, ring[j], ring[(j + 1) % sides], -1);
    }
  }
}

bool TreeMesher::Collar(int face,
                        const TreeSkeleton::Node &anchor,
                        const TreeSkeleton::Node &first,
                        Fitted within,
                        std::span<int> out) {
  if (face < 0 || (Dead_[static_cast<size_t>(face)] != 0u)) { return false; }
  const Face parent = Faces_[static_cast<size_t>(face)];
  const std::array<int, 4> o = {{parent.A, parent.B, parent.C, parent.D}};
  if (parent.D < 0) { return false; }

  const Vec3f ctr = anchor.Pos;
  const float r = anchor.Radius < within.RoomM ? anchor.Radius : within.RoomM;

  Dead_[static_cast<size_t>(face)] = 1;

  std::array<int, kMaxSides> inner{};
  for (int j = 0; j < within.Sides; ++j) {
    const float a = kTau * static_cast<float>(j) / static_cast<float>(within.Sides);
    inner[j] = AddVert(ctr + RingDir(anchor, a) * r);
  }
  Ring(first, first.Radius, within.Sides, out);
  Wall(inner, out, within.Sides);

  const Vec3f b = DirectionOrUp(Cross(anchor.Dir, anchor.Up));
  const Vec3f rel = Verts_[static_cast<size_t>(o[0])] - ctr;
  float a0 = std::atan2(Dot(rel, b), Dot(rel, anchor.Up));
  if (a0 < 0) { a0 += kTau; }
  int s =
      static_cast<int>(std::lround(a0 / kTau * static_cast<float>(within.Sides))) % within.Sides;
  if (s < 0) { s += within.Sides; }
  int ia = 0;
  int ib = 0;
  while (ia < 4 || ib < within.Sides) {
    const int oc = o[ia % 4];
    const float ta = static_cast<float>(ia) / 4.0f;
    const float tb = static_cast<float>(ib) / static_cast<float>(within.Sides);
    if (ib >= within.Sides || (ia < 4 && ta <= tb)) {
      AddFace(oc, o[(ia + 1) % 4], inner[(s + ib) % within.Sides], -1);
      ia++;
    } else {
      AddFace(oc, inner[(s + ib + 1) % within.Sides], inner[(s + ib) % within.Sides], -1);
      ib++;
    }
  }
  return true;
}

void TreeMesher::Draw(const TreeSkeleton &plant, float pixelHeightFrac, TreeMesh &out) {
  PixelGrow_ = pixelHeightFrac > 0.0f ? pixelHeightFrac : 0.0f;
  Verts_.clear();
  Faces_.clear();
  Dead_.clear();
  Bands_.assign(plant.Nodes.size(), Band{});
  Drawn_.assign(plant.Shoots.size(), 0);
  out.ClearBark();

  for (size_t i = 0; i < plant.Shoots.size(); ++i) {
    Drawn_[i] = plant.Shoots[i].Count >= 2 && plant.Shoots[i].Reach > PixelGrow_ ? 1 : 0;
  }

  std::array<int, kMaxSides> ring{};
  std::array<int, kMaxSides> next{};
  for (size_t i = 0; i < plant.Shoots.size(); ++i) {
    if (Drawn_[i] == 0u) { continue; }
    const TreeSkeleton::Shoot &shoot = plant.Shoots[i];
    const int last = shoot.First + shoot.Count - 1;
    const TreeSkeleton::Node &anchor = plant.Nodes[static_cast<size_t>(shoot.First)];
    const int sides = SidesFor({.RadiusM = anchor.Radius, .Declared = shoot.Sides});

    int face = -1;
    if (shoot.Parent >= 0 && (Drawn_[static_cast<size_t>(shoot.Parent)] != 0u)) {
      const Band band = Bands_[static_cast<size_t>(shoot.ParentNode)];
      if (band.First >= 0) {
        int k0 = static_cast<int>(shoot.Roll / kTau * static_cast<float>(band.Sides)) % band.Sides;
        if (k0 < 0) { k0 += band.Sides; }
        face = band.First + k0;
      }
    }
    int at = shoot.First;
    const int wall = static_cast<int>(Faces_.size());
    if (face >= 0 && Collar(face,
                            anchor,
                            plant.Nodes[static_cast<size_t>(shoot.First) + 1u],
                            {.Sides = sides, .RoomM = RoomAt(plant, shoot)},
                            ring)) {
      at = shoot.First + 1;
      Bands_[static_cast<size_t>(at)] = Band{.First = wall, .Sides = sides};
    } else {
      Ring(anchor, anchor.Radius, sides, ring);
      Cap(anchor, ring, sides, RingCap::Base, 0u);
    }
    RingsOf(plant, shoot, at);
    int covered = at;
    for (size_t station = 1; station < Stations_.size(); ++station) {
      const int n = Stations_[station];
      const int first = static_cast<int>(Faces_.size());
      Ring(plant.Nodes[static_cast<size_t>(n)],
           plant.Nodes[static_cast<size_t>(n)].Radius,
           sides,
           next);
      Wall(ring, next, sides);

      for (int skipped = covered + 1; skipped <= n; ++skipped) {
        Bands_[static_cast<size_t>(skipped)] = Band{.First = first, .Sides = sides};
      }
      covered = n;
      for (int j = 0; j < sides; ++j) { ring[j] = next[j]; }
    }

    Cap(plant.Nodes[static_cast<size_t>(last)],
        ring,
        sides,
        shoot.End,
        plant.Seed * kKnuthWord + static_cast<uint32_t>(i) + 1u);
  }

  Export(out);
}

void TreeMesher::Export(TreeMesh &out) {
  Normals_.assign(Verts_.size(), Vec3f{});
  for (size_t fi = 0; fi < Faces_.size(); ++fi) {
    if (Dead_[fi] != 0u) { continue; }
    const Face &f = Faces_[fi];
    const std::array<std::array<int, 3>, 2> tri = {{{f.A, f.B, f.C}, {f.A, f.C, f.D}}};
    const int nt = f.D < 0 ? 1 : 2;
    for (int ti = 0; ti < nt; ++ti) {
      const Vec3f a = Verts_[static_cast<size_t>(tri[ti][0])];
      const Vec3f b = Verts_[static_cast<size_t>(tri[ti][1])];
      const Vec3f c = Verts_[static_cast<size_t>(tri[ti][2])];
      const Vec3f fn = Cross(b - a, c - a);
      for (int e = 0; e < 3; ++e) {
        Normals_[static_cast<size_t>(tri[ti][e])] = Normals_[static_cast<size_t>(tri[ti][e])] + fn;
      }
    }
  }
  out.BarkVerts.resize(Verts_.size() * TreeMesh::kBarkFloats);
  for (size_t i = 0; i < Verts_.size(); ++i) {
    const Vec3f p = Verts_[i];
    const Vec3f n = DirectionOrUp(Normals_[i]);
    float *o = &out.BarkVerts[i * TreeMesh::kBarkFloats];
    o[0] = p[0];
    o[1] = p[1];
    o[2] = p[2];
    o[3] = n[0];
    o[4] = n[1];
    o[5] = n[2];
  }
  out.BarkIdx.clear();
  for (size_t fi = 0; fi < Faces_.size(); ++fi) {
    if (Dead_[fi] != 0u) { continue; }
    const Face &f = Faces_[fi];
    out.BarkIdx.push_back(static_cast<uint32_t>(f.A));
    out.BarkIdx.push_back(static_cast<uint32_t>(f.B));
    out.BarkIdx.push_back(static_cast<uint32_t>(f.C));
    if (f.D >= 0) {
      out.BarkIdx.push_back(static_cast<uint32_t>(f.A));
      out.BarkIdx.push_back(static_cast<uint32_t>(f.C));
      out.BarkIdx.push_back(static_cast<uint32_t>(f.D));
    }
  }
}

} // namespace outshine::Generators

#include "TreeMesher.h"

#include <numbers>
#include <algorithm>
#include <cmath>

#include "TreeRandom.h"

namespace outshine::Generators {

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;

TreeVec3 RingDir(const TreeSkeleton::Node &node, float angle) {
  const TreeVec3 b = Normalize(Cross(node.Dir, node.Up));
  return node.Up * std::cos(angle) + b * std::sin(angle);
}

} // namespace

int TreeMesher::AddVert(TreeVec3 p) {
  Verts_.push_back(p);
  return static_cast<int>(Verts_.size()) - 1;
}

int TreeMesher::AddFace(int a, int b, int c, int d) {
  Faces_.push_back(Face{a, b, c, d});
  Dead_.push_back(0);
  return static_cast<int>(Faces_.size()) - 1;
}

TreeVec3 TreeMesher::FaceCentroid(int fi) const {
  const Face &f = Faces_[static_cast<size_t>(fi)];
  const int n = f.D < 0 ? 3 : 4;
  const int idx[4] = {f.A, f.B, f.C, f.D};
  TreeVec3 s;
  for (int i = 0; i < n; ++i) { s = s + Verts_[static_cast<size_t>(idx[i])]; }
  return s * (1.0f / static_cast<float>(n));
}

int TreeMesher::SidesFor(float radius, int declared) const {
  int cap = declared < kMaxSides ? declared : kMaxSides;
  if (cap < 3) { cap = 3; }
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
    const TreeVec3 chord = nb.Pos - na.Pos;
    const float span = Dot(chord, chord);
    if (span <= 0.0f) { continue; }
    for (int i = a + 1; i < b; ++i) {
      const TreeSkeleton::Node &n = plant.Nodes[static_cast<size_t>(i)];
      const float t = Dot(n.Pos - na.Pos, chord) / span;
      const TreeVec3 off = n.Pos - (na.Pos + chord * t);

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
    for (int trial = 2; from + trial <= last && ChordHolds(plant, from, last, trial); trial <<= 1) {
      stride = trial;
    }
  }
  for (int i = last; i > from; i -= stride) { Stations_.push_back(i); }
  Stations_.push_back(from);
  std::reverse(Stations_.begin(), Stations_.end());
}

float TreeMesher::RoomAt(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot) const {
  const TreeSkeleton::Node &upper = plant.Nodes[static_cast<size_t>(shoot.ParentNode)];
  const TreeSkeleton::Node &lower = plant.Nodes[static_cast<size_t>(shoot.ParentNode - 1)];
  int sides = plant.Shoots[static_cast<size_t>(shoot.Parent)].Sides;
  if (sides > kMaxSides) { sides = kMaxSides; }
  if (sides < 3) { sides = 3; }
  const float along = 0.5f * Length(upper.Pos - lower.Pos);
  const float around = upper.Radius * std::sin(kTau * 0.5f / static_cast<float>(sides));
  return 0.9f * std::sqrt(along * along + around * around);
}

void TreeMesher::Ring(const TreeSkeleton::Node &node, float radius, int sides, int *out) {
  for (int j = 0; j < sides; ++j) {
    out[j] =
        AddVert(node.Pos +
                RingDir(node, kTau * static_cast<float>(j) / static_cast<float>(sides)) * radius);
  }
}

void TreeMesher::Wall(const int *from, const int *to, int sides) {
  for (int j = 0; j < sides; ++j) {
    AddFace(from[j], from[(j + 1) % sides], to[(j + 1) % sides], to[j]);
  }
}

void TreeMesher::BreakProfile(uint32_t seed, int sides, float *out) const {
  float splinter[kMaxSides];
  TreeRandom rng(seed);
  for (int i = 0; i < kMaxSides; ++i) { splinter[i] = rng.Unit(); }
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

void TreeMesher::Cap(
    const TreeSkeleton::Node &node, const int *ring, int sides, RingCap cap, uint32_t seed) {
  float apex = 0.0f;
  switch (cap) {
    case RingCap::Base: apex = -0.6f; break;
    case RingCap::Point: apex = 2.4f; break;
    case RingCap::Cut: apex = 0.0f; break;

    case RingCap::Broken: apex = 1.4f; break;
  }
  const int ci = AddVert(node.Pos + node.Dir * (node.Radius * apex));
  float splinter[kMaxSides] = {};
  if (cap == RingCap::Broken) { BreakProfile(seed, sides, splinter); }
  for (int j = 0; j < sides; ++j) {
    if (cap == RingCap::Broken) {
      Verts_[static_cast<size_t>(ring[j])] =
          Verts_[static_cast<size_t>(ring[j])] + node.Dir * (node.Radius * splinter[j] * 1.4f);
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
                        int sides,
                        float room,
                        int *out) {
  if (face < 0 || Dead_[static_cast<size_t>(face)]) { return false; }
  const Face parent = Faces_[static_cast<size_t>(face)];
  const int o[4] = {parent.A, parent.B, parent.C, parent.D};
  if (parent.D < 0) { return false; }

  const TreeVec3 ctr = anchor.Pos;
  const float r = anchor.Radius < room ? anchor.Radius : room;

  Dead_[static_cast<size_t>(face)] = 1;

  int inner[kMaxSides];
  for (int j = 0; j < sides; ++j) {
    const float a = kTau * static_cast<float>(j) / static_cast<float>(sides);
    inner[j] = AddVert(ctr + RingDir(anchor, a) * r);
  }
  Ring(first, first.Radius, sides, out);
  Wall(inner, out, sides);

  const TreeVec3 b = Normalize(Cross(anchor.Dir, anchor.Up));
  const TreeVec3 rel = Verts_[static_cast<size_t>(o[0])] - ctr;
  float a0 = std::atan2(Dot(rel, b), Dot(rel, anchor.Up));
  if (a0 < 0) { a0 += kTau; }
  int s = static_cast<int>(std::lround(a0 / kTau * static_cast<float>(sides))) % sides;
  if (s < 0) { s += sides; }
  int ia = 0, ib = 0;
  while (ia < 4 || ib < sides) {
    const int oc = o[ia % 4];
    const float ta = static_cast<float>(ia) / 4.0f,
                tb = static_cast<float>(ib) / static_cast<float>(sides);
    if (ib >= sides || (ia < 4 && ta <= tb)) {
      AddFace(oc, o[(ia + 1) % 4], inner[(s + ib) % sides], -1);
      ia++;
    } else {
      AddFace(oc, inner[(s + ib + 1) % sides], inner[(s + ib) % sides], -1);
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

  int ring[kMaxSides], next[kMaxSides];
  for (size_t i = 0; i < plant.Shoots.size(); ++i) {
    if (!Drawn_[i]) { continue; }
    const TreeSkeleton::Shoot &shoot = plant.Shoots[i];
    const int last = shoot.First + shoot.Count - 1;
    const TreeSkeleton::Node &anchor = plant.Nodes[static_cast<size_t>(shoot.First)];
    const int sides = SidesFor(anchor.Radius, shoot.Sides);

    int face = -1;
    if (shoot.Parent >= 0 && Drawn_[static_cast<size_t>(shoot.Parent)]) {
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
                            plant.Nodes[static_cast<size_t>(shoot.First + 1)],
                            sides,
                            RoomAt(plant, shoot),
                            ring)) {
      at = shoot.First + 1;
      Bands_[static_cast<size_t>(at)] = Band{wall, sides};
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
        Bands_[static_cast<size_t>(skipped)] = Band{first, sides};
      }
      covered = n;
      for (int j = 0; j < sides; ++j) { ring[j] = next[j]; }
    }

    Cap(plant.Nodes[static_cast<size_t>(last)],
        ring,
        sides,
        shoot.End,
        plant.Seed * 2654435761u + static_cast<uint32_t>(i) + 1u);
  }

  Export(out);
}

void TreeMesher::Export(TreeMesh &out) {
  Normals_.assign(Verts_.size(), TreeVec3{});
  for (size_t fi = 0; fi < Faces_.size(); ++fi) {
    if (Dead_[fi]) { continue; }
    const Face &f = Faces_[fi];
    const int tri[2][3] = {{f.A, f.B, f.C}, {f.A, f.C, f.D}};
    const int nt = f.D < 0 ? 1 : 2;
    for (int ti = 0; ti < nt; ++ti) {
      const TreeVec3 a = Verts_[static_cast<size_t>(tri[ti][0])],
                     b = Verts_[static_cast<size_t>(tri[ti][1])],
                     c = Verts_[static_cast<size_t>(tri[ti][2])];
      const TreeVec3 fn = Cross(b - a, c - a);
      for (int e = 0; e < 3; ++e) {
        Normals_[static_cast<size_t>(tri[ti][e])] = Normals_[static_cast<size_t>(tri[ti][e])] + fn;
      }
    }
  }
  out.BarkVerts.resize(Verts_.size() * TreeMesh::kBarkFloats);
  for (size_t i = 0; i < Verts_.size(); ++i) {
    const TreeVec3 p = Verts_[i];
    const TreeVec3 n = Normalize(Normals_[i]);
    float *o = &out.BarkVerts[i * TreeMesh::kBarkFloats];
    o[0] = p.X;
    o[1] = p.Y;
    o[2] = p.Z;
    o[3] = n.X;
    o[4] = n.Y;
    o[5] = n.Z;
  }
  out.BarkIdx.clear();
  for (size_t fi = 0; fi < Faces_.size(); ++fi) {
    if (Dead_[fi]) { continue; }
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

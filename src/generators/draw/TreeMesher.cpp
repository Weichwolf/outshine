#include "TreeMesher.h"

#include <algorithm>
#include <cmath>

#include "TreeRandom.h"

namespace outshine::Generators {

namespace {

constexpr float kTau = 6.2831853f;

/* A ring's j-th vertex direction, and the ONE place that angle is spelled. A shoot's declared roll
 * indexes the same circle, which is what lets a branch find the wall it grew through. */
TreeVec3 RingDir(const TreeSkeleton::Node &node, float angle) {
  const TreeVec3 b = Normalize(Cross(node.Dir, node.Up));
  return node.Up * std::cos(angle) + b * std::sin(angle);
}

} // namespace

int TreeMesher::AddVert(TreeVec3 p) {
  Verts_.push_back(p);
  return (int)Verts_.size() - 1;
}

int TreeMesher::AddFace(int a, int b, int c, int d) {
  Faces_.push_back(Face{a, b, c, d});
  Dead_.push_back(0);
  return (int)Faces_.size() - 1;
}

TreeVec3 TreeMesher::FaceCentroid(int fi) const {
  const Face &f = Faces_[(size_t)fi];
  const int n = f.D < 0 ? 3 : 4;
  const int idx[4] = {f.A, f.B, f.C, f.D};
  TreeVec3 s;
  for (int i = 0; i < n; ++i) { s = s + Verts_[(size_t)idx[i]]; }
  return s * (1.0f / (float)n);
}

int TreeMesher::SidesFor(float radius, int declared) const {
  int cap = declared < kMaxSides ? declared : kMaxSides;
  if (cap < 3) { cap = 3; }
  if (PixelGrow_ <= 0.0f || radius <= 0.0f) { return cap; }
  const float c = 1.0f - 0.5f * PixelGrow_ / radius;
  if (c <= -1.0f) { return 3; }
  const int n = (int)std::ceil(3.14159265f / std::acos(c));
  if (n < 3) { return 3; }
  return n < cap ? n : cap;
}

bool TreeMesher::ChordHolds(const TreeSkeleton &plant, int from, int last, int stride) const {
  const float tol = 0.5f * PixelGrow_;
  for (int b = last; b - stride >= from; b -= stride) {
    const int a = b - stride;
    const TreeSkeleton::Node &na = plant.Nodes[(size_t)a];
    const TreeSkeleton::Node &nb = plant.Nodes[(size_t)b];
    const TreeVec3 chord = nb.Pos - na.Pos;
    const float span = Dot(chord, chord);
    if (span <= 0.0f) { continue; }
    for (int i = a + 1; i < b; ++i) {
      const TreeSkeleton::Node &n = plant.Nodes[(size_t)i];
      const float t = Dot(n.Pos - na.Pos, chord) / span;
      const TreeVec3 off = n.Pos - (na.Pos + chord * t);
      /* An axis that has moved and a radius that has changed both move the outline, so the budget
       * pays for their sum rather than for each alone. */
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

/* Half the diagonal of the FINEST wall the parent can have where this shoot leaves it: half a
 * skeleton segment along the axis, one side of the parent's declared polygon around it. */
float TreeMesher::RoomAt(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot) const {
  const TreeSkeleton::Node &upper = plant.Nodes[(size_t)shoot.ParentNode];
  const TreeSkeleton::Node &lower = plant.Nodes[(size_t)(shoot.ParentNode - 1)];
  int sides = plant.Shoots[(size_t)shoot.Parent].Sides;
  if (sides > kMaxSides) { sides = kMaxSides; }
  if (sides < 3) { sides = 3; }
  const float along = 0.5f * Length(upper.Pos - lower.Pos);
  const float around = upper.Radius * std::sin(kTau * 0.5f / (float)sides);
  return 0.9f * std::sqrt(along * along + around * around);
}

void TreeMesher::Ring(const TreeSkeleton::Node &node, float radius, int sides, int *out) {
  for (int j = 0; j < sides; ++j) {
    out[j] = AddVert(node.Pos + RingDir(node, kTau * (float)j / (float)sides) * radius);
  }
}

void TreeMesher::Wall(const int *from, const int *to, int sides) {
  for (int j = 0; j < sides; ++j) {
    AddFace(from[j], from[(j + 1) % sides], to[(j + 1) % sides], to[j]);
  }
}

/* THE FRACTURE IS A PROFILE ROUND THE RING, NOT A VALUE PER VERTEX. Drawn per index it moved when
 * the side count did, and a coarser rank then stood TALLER than a finer one — measured on log_beech
 * and snag_spruce, the only two declarations that break. Drawn instead at the finest ring this rule
 * allows and taken as the LOWEST splinter the vertex stands for, a coarser ring can only fall short
 * of a finer one, which is the direction a refinement is allowed to move in. */
void TreeMesher::BreakProfile(uint32_t seed, int sides, float *out) const {
  float splinter[kMaxSides];
  TreeRandom rng(seed);
  for (int i = 0; i < kMaxSides; ++i) { splinter[i] = rng.Unit(); }
  for (int j = 0; j < sides; ++j) {
    const float at = kTau * (float)j / (float)sides;
    const float arc = kTau * 0.5f / (float)sides;
    float lowest = 1.0f;
    for (int i = 0; i < kMaxSides; ++i) {
      float apart = std::fabs(kTau * (float)i / (float)kMaxSides - at);
      if (apart > kTau * 0.5f) { apart = kTau - apart; }
      if (apart <= arc && splinter[i] < lowest) { lowest = splinter[i]; }
    }
    out[j] = lowest;
  }
}

void TreeMesher::Cap(const TreeSkeleton::Node &node, const int *ring, int sides, RingCap cap,
                     uint32_t seed) {
  float apex = 0.0f;
  switch (cap) {
    case RingCap::Base: apex = -0.6f; break;
    case RingCap::Point: apex = 2.4f; break;
    case RingCap::Cut: apex = 0.0f; break;
    /* A wind break is splinters, not a plane: the ring's own vertices are pulled forward at random
     * and the apex stands at the longest splinter of all — which is both what a snapped trunk leaves
     * and the only way the TOPMOST point of the cap sits on the axis. A rim vertex there instead
     * made the plant's height depend on how many sides the ring had, because a k-gon inscribed in a
     * circle does not sample the circle's highest point monotonically in k. */
    case RingCap::Broken: apex = 1.4f; break;
  }
  const int ci = AddVert(node.Pos + node.Dir * (node.Radius * apex));
  float splinter[kMaxSides] = {};
  if (cap == RingCap::Broken) { BreakProfile(seed, sides, splinter); }
  for (int j = 0; j < sides; ++j) {
    if (cap == RingCap::Broken) {
      Verts_[(size_t)ring[j]] =
          Verts_[(size_t)ring[j]] + node.Dir * (node.Radius * splinter[j] * 1.4f);
    }
    if (cap == RingCap::Base) {
      AddFace(ci, ring[(j + 1) % sides], ring[j], -1);
    } else {
      AddFace(ci, ring[j], ring[(j + 1) % sides], -1);
    }
  }
}

bool TreeMesher::Collar(int face, const TreeSkeleton::Node &anchor,
                        const TreeSkeleton::Node &first, int sides, float room, int *out) {
  if (face < 0 || Dead_[(size_t)face]) { return false; }
  const Face parent = Faces_[(size_t)face];
  const int o[4] = {parent.A, parent.B, parent.C, parent.D};
  if (parent.D < 0) { return false; }
  /* NO DRAWN VERTEX MAY DEPEND ON ANOTHER DRAWN VERTEX. The collar sits where the plant says the
   * shoot leaves and is no wider than the NARROWEST wall this rule could leave there; both come off
   * the skeleton. Reading the wall that happened to be standing instead made a coarse rank of a snag
   * 2.6 cm TALLER than a fine one (measured, snag_spruce), because a decimated wall spans several
   * stations, its middle is not the anchor and its corners are further out. */
  const TreeVec3 ctr = anchor.Pos;
  const float r = anchor.Radius < room ? anchor.Radius : room;

  Dead_[(size_t)face] = 1;

  /* A REGULAR polygon at the collar AND at the first ring: equal segments, no distortion where the
   * branch leaves the trunk. */
  int inner[kMaxSides];
  for (int j = 0; j < sides; ++j) {
    const float a = kTau * (float)j / (float)sides;
    inner[j] = AddVert(ctr + RingDir(anchor, a) * r);
  }
  Ring(first, first.Radius, sides, out);
  Wall(inner, out, sides);

  /* The parent's quad stitched to the branch's polygon by angle, so the hole the dead face left is
   * closed without a T-junction. */
  const TreeVec3 b = Normalize(Cross(anchor.Dir, anchor.Up));
  const TreeVec3 rel = Verts_[(size_t)o[0]] - ctr;
  float a0 = std::atan2(Dot(rel, b), Dot(rel, anchor.Up));
  if (a0 < 0) { a0 += kTau; }
  int s = (int)std::lround(a0 / kTau * (float)sides) % sides;
  if (s < 0) { s += sides; }
  int ia = 0, ib = 0;
  while (ia < 4 || ib < sides) {
    const int oc = o[ia % 4];
    const float ta = (float)ia / 4.0f, tb = (float)ib / (float)sides;
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
    const TreeSkeleton::Node &anchor = plant.Nodes[(size_t)shoot.First];
    const int sides = SidesFor(anchor.Radius, shoot.Sides);

    int face = -1;
    if (shoot.Parent >= 0 && Drawn_[(size_t)shoot.Parent]) {
      const Band band = Bands_[(size_t)shoot.ParentNode];
      if (band.First >= 0) {
        int k0 = (int)(shoot.Roll / kTau * (float)band.Sides) % band.Sides;
        if (k0 < 0) { k0 += band.Sides; }
        face = band.First + k0;
      }
    }
    int at = shoot.First;
    const int wall = (int)Faces_.size();
    if (face >= 0 && Collar(face, anchor, plant.Nodes[(size_t)(shoot.First + 1)], sides,
                            RoomAt(plant, shoot), ring)) {
      at = shoot.First + 1;
      Bands_[(size_t)at] = Band{wall, sides};
    } else {
      Ring(anchor, anchor.Radius, sides, ring);
      Cap(anchor, ring, sides, RingCap::Base, 0u);
    }
    RingsOf(plant, shoot, at);
    int covered = at;
    for (size_t station = 1; station < Stations_.size(); ++station) {
      const int n = Stations_[station];
      const int first = (int)Faces_.size();
      Ring(plant.Nodes[(size_t)n], plant.Nodes[(size_t)n].Radius, sides, next);
      Wall(ring, next, sides);
      /* A STATION WITH NO RING OF ITS OWN STILL HAS TO ANSWER A CHILD, so the band that spans it is
       * the one its branches leave through. Without this a decimated shoot would orphan every branch
       * it carries and the crown would come apart at exactly the ranks meant to be cheaper. */
      for (int skipped = covered + 1; skipped <= n; ++skipped) {
        Bands_[(size_t)skipped] = Band{first, sides};
      }
      covered = n;
      for (int j = 0; j < sides; ++j) { ring[j] = next[j]; }
    }
    /* The break is random and the randomness is the PLANT's, not the drawing's: seeded from the
     * declaration and the shoot, it splinters the same way however many sides the ring has. */
    Cap(plant.Nodes[(size_t)last], ring, sides, shoot.End,
        plant.Seed * 2654435761u + (uint32_t)i + 1u);
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
      const TreeVec3 a = Verts_[(size_t)tri[ti][0]], b = Verts_[(size_t)tri[ti][1]],
                     c = Verts_[(size_t)tri[ti][2]];
      const TreeVec3 fn = Cross(b - a, c - a);
      for (int e = 0; e < 3; ++e) {
        Normals_[(size_t)tri[ti][e]] = Normals_[(size_t)tri[ti][e]] + fn;
      }
    }
  }
  out.BarkVerts.resize(Verts_.size() * TreeMesh::kBarkFloats);
  for (size_t i = 0; i < Verts_.size(); ++i) {
    const TreeVec3 p = Verts_[i];
    const TreeVec3 n = Normalize(Normals_[i]);
    float *o = &out.BarkVerts[i * TreeMesh::kBarkFloats];
    o[0] = p.X; o[1] = p.Y; o[2] = p.Z;
    o[3] = n.X; o[4] = n.Y; o[5] = n.Z;
  }
  out.BarkIdx.clear();
  for (size_t fi = 0; fi < Faces_.size(); ++fi) {
    if (Dead_[fi]) { continue; }
    const Face &f = Faces_[fi];
    out.BarkIdx.push_back((uint32_t)f.A);
    out.BarkIdx.push_back((uint32_t)f.B);
    out.BarkIdx.push_back((uint32_t)f.C);
    if (f.D >= 0) {
      out.BarkIdx.push_back((uint32_t)f.A);
      out.BarkIdx.push_back((uint32_t)f.C);
      out.BarkIdx.push_back((uint32_t)f.D);
    }
  }
}

} // namespace outshine::Generators

#include "TreeGrower.h"

#include <cmath>

namespace outshine::World {

namespace {

constexpr float kTau = 6.2831853f;
constexpr float kGolden = 2.39996323f; /* the divergence angle, in radians — phyllotaxis along a shoot */
constexpr float kDeg = 0.01745f;

} // namespace

int TreeGrower::AddVert(TreeVec3 p) {
  Verts_.push_back(p);
  return (int)Verts_.size() - 1;
}

int TreeGrower::AddFace(int a, int b, int c, int d) {
  Faces_.push_back(Face{a, b, c, d});
  Dead_.push_back(0);
  return (int)Faces_.size() - 1;
}

TreeVec3 TreeGrower::FaceNormal(int fi) const {
  const Face &f = Faces_[(size_t)fi];
  const TreeVec3 a = Verts_[(size_t)f.A], b = Verts_[(size_t)f.B], c = Verts_[(size_t)f.C];
  return Normalize(Cross(b - a, c - a));
}

/* A regular n-gon of radius r misses its circle by r(1 - cos(pi/n)); half a pixel of that is the whole
 * budget, and the species' own `trunk_sides` stays the ceiling — the rule may make a tube coarser,
 * never rounder than the declaration. */
int TreeGrower::SidesFor(float radius, int declared) const {
  int cap = declared < kMaxSides ? declared : kMaxSides;
  if (cap < 3) { cap = 3; }
  if (PixelGrow_ <= 0.0f || radius <= 0.0f) { return cap; }
  const float c = 1.0f - 0.5f * PixelGrow_ / radius;
  if (c <= -1.0f) { return 3; }
  const int n = (int)std::ceil(3.14159265f / std::acos(c));
  if (n < 3) { return 3; }
  return n < cap ? n : cap;
}

TreeVec3 TreeGrower::FaceCentroid(int fi) const {
  const Face &f = Faces_[(size_t)fi];
  const int n = f.D < 0 ? 3 : 4;
  const int idx[4] = {f.A, f.B, f.C, f.D};
  TreeVec3 s;
  for (int i = 0; i < n; ++i) { s = s + Verts_[(size_t)idx[i]]; }
  return s * (1.0f / (float)n);
}

int TreeGrower::ExtrudeCap(Tip &t, TreeVec3 oldDir, float step, float radius, int *ringOut) {
  const TreeVec3 nPos = t.Pos + t.Dir * step;
  TreeVec3 n = RmfDouble(t.Pos, nPos, oldDir, t.Dir, t.Up);
  n = Normalize(n - t.Dir * Dot(n, t.Dir));
  const TreeVec3 b = Normalize(Cross(t.Dir, n));
  t.Up = n;
  const int k = t.K;
  for (int i = 0; i < k; ++i) {
    const float ang = kTau * (float)i / (float)k;
    const TreeVec3 d = n * std::cos(ang) + b * std::sin(ang);
    ringOut[i] = AddVert(nPos + d * radius);
  }
  const int first = (int)Faces_.size();
  for (int i = 0; i < k; ++i) {
    AddFace(t.Ring[i], t.Ring[(i + 1) % k], ringOut[(i + 1) % k], ringOut[i]);
  }
  t.Pos = nPos;
  t.Radius = radius;
  return first;
}

TreeGrower::Tip TreeGrower::BranchFromFace(int fi, TreeVec3 dir, float radius, float step,
                                           int sides) {
  const Face parent = Faces_[(size_t)fi];
  const int o[4] = {parent.A, parent.B, parent.C, parent.D};
  const TreeVec3 ctr = FaceCentroid(fi);
  const TreeVec3 up = std::fabs(dir.Y) < 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
  TreeVec3 n, b;
  FrameFrom(dir, up, n, b);

  float minD = 1e30f;
  for (int i = 0; i < 4; ++i) {
    const float d = Length(Verts_[(size_t)o[i]] - ctr);
    if (d < minD) { minD = d; }
  }
  const float r = radius < minD * 0.9f ? radius : minD * 0.9f;

  Dead_[(size_t)fi] = 1;

  /* A REGULAR polygon at the collar AND at the first ring: equal segments, no distortion where the
   * branch leaves the trunk. */
  const int kb = sides;
  int inner[kMaxSides], ring[kMaxSides];
  const TreeVec3 nPos = ctr + dir * step;
  for (int j = 0; j < kb; ++j) {
    const float a = kTau * (float)j / (float)kb;
    const TreeVec3 d = n * std::cos(a) + b * std::sin(a);
    inner[j] = AddVert(ctr + d * r);
    ring[j] = AddVert(nPos + d * r);
  }
  for (int j = 0; j < kb; ++j) { AddFace(inner[j], inner[(j + 1) % kb], ring[(j + 1) % kb], ring[j]); }

  /* The collar: the parent's quad stitched to the octagon by angle, so the hole the dead face left is
   * closed without a T-junction. */
  const TreeVec3 rel = Verts_[(size_t)o[0]] - ctr;
  float a0 = std::atan2(Dot(rel, b), Dot(rel, n));
  if (a0 < 0) { a0 += kTau; }
  int s = (int)std::lround(a0 / kTau * (float)kb) % kb;
  if (s < 0) { s += kb; }
  int ia = 0, ib = 0;
  while (ia < 4 || ib < kb) {
    const int oc = o[ia % 4];
    const float ta = (float)ia / 4.0f, tb = (float)ib / (float)kb;
    if (ib >= kb || (ia < 4 && ta <= tb)) {
      AddFace(oc, o[(ia + 1) % 4], inner[(s + ib) % kb], -1);
      ia++;
    } else {
      AddFace(oc, inner[(s + ib + 1) % kb], inner[(s + ib) % kb], -1);
      ib++;
    }
  }

  Tip out;
  out.K = kb;
  for (int j = 0; j < kb; ++j) { out.Ring[j] = ring[j]; }
  out.Dir = dir;
  out.Up = n;
  out.Pos = nPos;
  out.Radius = r;
  out.Roll = 0.0f;
  return out;
}

void TreeGrower::CapRing(const Tip &t, bool forward) {
  if (t.K == 0) { return; }
  const TreeVec3 c = t.Pos + t.Dir * (t.Radius * (forward ? 2.4f : -0.6f));
  const int ci = AddVert(c);
  for (int k = 0; k < t.K; ++k) {
    if (forward) {
      AddFace(ci, t.Ring[k], t.Ring[(k + 1) % t.K], -1);
    } else {
      AddFace(ci, t.Ring[(k + 1) % t.K], t.Ring[k], -1);
    }
  }
}

/* The finest shoot level carries no geometry: it distributes attachment points ON the shoot surface
 * with an outward stalk vector. Every leaf therefore sits on a twig and grows away from it. */
void TreeGrower::EmitLeafPoints(TreeMesh &out, TreeVec3 pos, TreeVec3 dir, TreeVec3 up, float radius,
                                int count, float roll) {
  TreeVec3 n, b;
  FrameFrom(dir, up, n, b);
  if (count < 1) { count = 1; }
  for (int i = 0; i < count; ++i) {
    const float az = roll + kTau * ((float)i / (float)count) + Rng_.Signed() * 0.30f;
    const TreeVec3 radial = n * std::cos(az) + b * std::sin(az);
    out.LeafPoints.push_back(TreeMesh::LeafPoint{pos + radial * (radius * 0.95f),
                                                 Normalize(radial + dir * 0.40f)});
  }
}

void TreeGrower::SpawnLateral(const Tip &t, const TreeSpecies::Growth &g, int first, float roll,
                              int step) {
  if (t.K == 0) { return; }
  int k0 = (int)(roll / kTau * (float)t.K) % t.K;
  if (k0 < 0) { k0 += t.K; }
  const int fi = first + k0;
  if (Dead_[(size_t)fi]) { return; }
  const TreeVec3 fn = FaceNormal(fi);
  const float a = (g.BranchAngle + Rng_.Signed() * g.BranchAngleVar) * kDeg;
  const TreeVec3 dir = Normalize(t.Dir * std::cos(a) + fn * std::sin(a));
  const float br = t.Radius * g.OrderRadius;
  if (br <= g.MinRadius) { return; }
  /* Shade pruning: a second-order branch growing back INTO the crown is shaded, so it is dropped with
   * the declared probability — a closed outer shell instead of a visible inner skeleton. */
  if (g.ShadePrune > 0.0f && t.Order >= 2) {
    const float rl = std::sqrt(t.Pos.X * t.Pos.X + t.Pos.Z * t.Pos.Z);
    if (rl > 1e-4f) {
      const float dOut = (dir.X * t.Pos.X + dir.Z * t.Pos.Z) / rl;
      if (dOut < -0.05f && Rng_.Unit() < g.ShadePrune) { return; }
    }
  }
  Tip b;
  if (2.0f * br <= PixelGrow_) {
    b.K = 0;
    b.Dir = dir;
    b.Up = FaceNormal(fi);
    b.Pos = FaceCentroid(fi);
    b.Radius = br;
  } else {
    b = BranchFromFace(fi, dir, br, g.StepLen, SidesFor(br, kBranchSides));
  }
  b.Order = t.Order + 1;
  float cLen = g.OrderLen;
  if (t.Order == 0 && g.Conical > 0.0f) {
    cLen *= (1.0f - g.Conical * ((float)step / (float)t.Steps));
  }
  b.Steps = (int)((float)t.Steps * cLen);
  if (b.Steps < 3) { b.Steps = 3; }
  b.Bare = 1;
  b.Foliate = t.Foliate && (t.Order != 0 || step >= (int)(g.CrownBase * (float)t.Steps));
  Queue_.push_back(b);
}

/* THE STEM IS SOLVED, NOT DECLARED. `base_radius` lives in grower units and the mesh is normalised by
 * a height the branches decide, so the same 0.08 came out as 70 cm on a beech and 80 cm on an elm —
 * measured. The species declares the number forestry measures (`bhd_cm`) and the whole radius cascade
 * is scaled to hit it; scaling base, twig and min together keeps every branch's termination and every
 * leaf point exactly where they were, so only the thickness moves. */
void TreeGrower::Grow(const TreeSpecies &species, TreeMesh &out, float pixelHeightFrac) {
  TreeSpecies::Growth g = species.GrowthParams();
  const float h = species.HeightM();
  const float targetR = species.BhdM() * 0.5f;

  /* The rule arrives in tree HEIGHTS and the grower works in its own units, so the first pass is a
   * CALIBRATION: it grows the declared mesh only to learn what height that is. `trunk_steps *
   * step_len` will not do — the crown reaches well past the leader's own run (buche 6,80 against
   * 4,16, eiche 4,54 against 2,24, both measured), and a pixel 60 % too small buys detail nobody
   * sees. */
  PixelGrow_ = 0.0f;
  NormHeight_ = 0.0f;
  Passes_ = 1;
  GrowOnce(g, h, out);
  if (pixelHeightFrac > 0.0f) {
    PixelGrow_ = pixelHeightFrac * GrowHeight_;
    /* EVERY RANK IS THE SAME TREE AND MUST BE THE SAME SIZE. A coarse rank has no bark on its
     * topmost shoots, so its own box is up to a quarter shorter (measured, buche rank 2: 5,08 grower
     * units against 6,80) — normalising by that box would make the tree jump when the rank changes. */
    NormHeight_ = GrowHeight_;
    GrowOnce(g, h, out);
    Passes_++;
  }
  BhdErrorRel_ = 0.0f;
  if (targetR <= 0.0f || h <= 0.0f) { return; }

  for (int i = 0; i < 4; ++i) {
    const float haveR = out.BhdRadius * h;
    BhdErrorRel_ = haveR > 0.0f ? (haveR - targetR) / targetR : 0.0f;
    if (haveR <= 0.0f || std::fabs(BhdErrorRel_) < 0.005f) { break; }
    const float f = targetR / haveR;
    g.BaseRadius *= f;
    g.MinRadius *= f;
    g.TwigRadius *= f;
    GrowOnce(g, h, out);
    Passes_++;
  }
}

void TreeGrower::GrowOnce(const TreeSpecies::Growth &g, float heightM, TreeMesh &out) {
  Verts_.clear();
  Faces_.clear();
  Dead_.clear();
  Queue_.clear();
  TrunkProfile_.clear();
  out.Clear();
  Rng_ = TreeRandom(g.Seed);

  const float leafThreshold = g.TwigRadius * g.FoliageFactor;

  const int k = SidesFor(g.BaseRadius, g.TrunkSides);
  Tip trunk;
  trunk.K = k;
  trunk.Dir = Vec3(0, 1, 0);
  trunk.Up = Vec3(0, 0, 1);
  trunk.Pos = Vec3(0, 0, 0);
  trunk.Radius = g.BaseRadius;
  trunk.Order = 0;
  trunk.Steps = g.TrunkSteps;
  trunk.Bare = g.BareSteps;
  trunk.Roll = 0.0f;
  trunk.Foliate = true;
  {
    TreeVec3 n, b;
    FrameFrom(trunk.Dir, trunk.Up, n, b);
    for (int i = 0; i < k; ++i) {
      const float ang = kTau * (float)i / (float)k;
      const TreeVec3 d = n * std::cos(ang) + b * std::sin(ang);
      trunk.Ring[i] = AddVert(trunk.Pos + d * trunk.Radius);
    }
  }
  TrunkProfile_.push_back(Vec3(trunk.Pos.Y, trunk.Radius, 0.0f));
  CapRing(trunk, false);

  Queue_.push_back(trunk);

  for (size_t head = 0; head < Queue_.size(); ++head) {
    Tip t = Queue_[head];
    int newRing[kMaxSides];
    int lastFirst = -1;
    float leafRoll = t.Roll;
    for (int s = 0; s < t.Steps; ++s) {
      if ((int)Verts_.size() > kVertexCeiling) { break; }
      const TreeVec3 oldDir = t.Dir;
      /* Wander plus tropism: the leader holds its own axis (LeaderBias), a branch follows
       * BranchUpBias, which is negative for a weeping habit. */
      TreeVec3 nf, bf;
      FrameFrom(t.Dir, t.Up, nf, bf);
      const float wr = g.Wander * kDeg;
      const float ub = (t.Order == 0) ? g.LeaderBias : g.BranchUpBias;
      TreeVec3 want = t.Dir;
      want = want + nf * (Rng_.Signed() * wr);
      want = want + bf * (Rng_.Signed() * wr);
      want = want + Vec3(0, 1, 0) * ub;
      t.Dir = Normalize(want);

      const float nr = t.Radius * g.Taper;
      /* The rule follows the TAPER, not just the spawn: a shoot that starts a pixel wide and thins
       * over its run would otherwise carry its spawn ring to its tip. It is capped where it crosses
       * and goes on as a point tip, so its leaves stay where they grew. */
      if (t.K > 0 && 2.0f * nr <= PixelGrow_) {
        CapRing(t, true);
        t.K = 0;
        lastFirst = -1;
      }
      int first = -1;
      if (t.K > 0) {
        first = ExtrudeCap(t, oldDir, g.StepLen, nr, newRing);
        lastFirst = first;
        for (int i = 0; i < t.K; ++i) { t.Ring[i] = newRing[i]; }
      } else {
        t.Pos = t.Pos + t.Dir * g.StepLen;
        t.Radius = nr;
      }
      if (t.Order == 0) { TrunkProfile_.push_back(Vec3(t.Pos.Y, t.Radius, 0.0f)); }

      const bool leaderOk = (t.Order != 0) || (s >= (int)(g.CrownBase * (float)t.Steps));
      const bool foliated = t.Foliate && leaderOk &&
                            ((t.Order >= 1) || (g.FoliageOnLeader && t.Radius < leafThreshold));
      if (foliated && t.Radius < leafThreshold) {
        leafRoll += kGolden;
        EmitLeafPoints(out, t.Pos, t.Dir, t.Up, t.Radius, 3, leafRoll);
      }

      if (t.Bare > 0) {
        t.Bare--;
      } else if (t.Order < g.MaxOrder && (int)Verts_.size() < kSpawnCeiling) {
        if (g.WhorlCount > 0 && t.Order == 0) {
          if (((s - g.BareSteps) % g.WhorlSpacing) == 0) {
            for (int wb = 0; wb < g.WhorlCount; ++wb) {
              SpawnLateral(t, g, first, (float)wb * kTau / (float)g.WhorlCount + Rng_.Signed() * 0.25f,
                           s);
            }
          }
        } else if (Rng_.Unit() < g.BranchChance) {
          t.Roll += kGolden + Rng_.Signed() * 0.4f;
          SpawnLateral(t, g, first, t.Roll, s);
        }
      }
      if (t.Radius < g.MinRadius) { break; }
    }
    /* A shoot still thick enough forks into two instead of ending in a flat cap. */
    if (g.TerminalFork && lastFirst >= 0 && t.Radius > g.TwigRadius && t.Order <= g.MaxOrder &&
        (int)Verts_.size() < kSpawnCeiling) {
      const int km[2] = {0, t.K / 2};
      for (int j = 0; j < 2; ++j) {
        const int fi = lastFirst + km[j];
        if (Dead_[(size_t)fi]) { continue; }
        const TreeVec3 fn = FaceNormal(fi);
        const TreeVec3 dir = Normalize(t.Dir + fn * 0.55f);
        const float br = t.Radius * 0.74f;
        Tip b;
        if (2.0f * br <= PixelGrow_) {
          b.K = 0;
          b.Dir = dir;
          b.Up = fn;
          b.Pos = FaceCentroid(fi);
          b.Radius = br;
        } else {
          b = BranchFromFace(fi, dir, br, g.StepLen, SidesFor(br, kBranchSides));
        }
        b.Order = t.Order + 1;
        b.Steps = (int)((float)t.Steps * g.OrderLen);
        if (b.Steps < 3) { b.Steps = 3; }
        b.Bare = 1;
        b.Foliate = t.Foliate;
        Queue_.push_back(b);
      }
    }
    CapRing(t, true);
    if (t.Foliate && ((t.Order >= 1) || (g.FoliageOnLeader && t.Radius < leafThreshold * 1.7f)) &&
        t.Radius < leafThreshold * 1.7f) {
      EmitLeafPoints(out, t.Pos, t.Dir, t.Up, t.Radius, 4, leafRoll + 1.1f);
    }
  }

  Export(out);
  NormalizeToUnitHeight(out, heightM);
}

void TreeGrower::Export(TreeMesh &out) const {
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
    o[6] = 0.0f; o[7] = p.Y;
    o[8] = 0.0f; o[9] = 1.0f; o[10] = 0.0f;
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

/* THE ORIGIN IS THE TRUNK FOOT, and the crown's bounding box has no say in it. Centring x/z on the box
 * put the stem up to 5.65 m (kiefer, measured) off the point the scatter placed the tree at, and yaw
 * then swung the whole tree around that point on that radius. The trunk grows from (0, ·, 0), so the
 * only thing to do is not to move it. */
void TreeGrower::NormalizeToUnitHeight(TreeMesh &out, float heightM) {
  if (out.BarkVerts.empty()) { return; }
  TreeVec3 mn = Vec3(1e30f, 1e30f, 1e30f), mx = Vec3(-1e30f, -1e30f, -1e30f);
  for (size_t i = 0; i < out.BarkVertexCount(); ++i) {
    const float *v = &out.BarkVerts[i * TreeMesh::kBarkFloats];
    mn = Vec3(std::fmin(mn.X, v[0]), std::fmin(mn.Y, v[1]), std::fmin(mn.Z, v[2]));
    mx = Vec3(std::fmax(mx.X, v[0]), std::fmax(mx.Y, v[1]), std::fmax(mx.Z, v[2]));
  }
  /* Y = 0 IS THE TRUNK FOOT, not the lowest vertex, and `height_m` is measured from there — which is
   * what a stand height is. Taking the mesh minimum put a Trauerweide's foot 6,87 m and a Fichte's
   * 3,67 m above the ground (measured), because both hang branches below their own base. A branch
   * below zero belongs below the terrain; that is where it grows. */
  const float y0 = TrunkProfile_.empty() ? mn.Y : TrunkProfile_[0].X - 0.6f * TrunkProfile_[0].Y;
  float h = mx.Y - y0;
  if (h < 1e-6f) { h = 1.0f; }
  GrowHeight_ = h;
  if (NormHeight_ > 0.0f) { h = NormHeight_; }
  const float s = 1.0f / h;
  for (size_t i = 0; i < out.BarkVertexCount(); ++i) {
    float *v = &out.BarkVerts[i * TreeMesh::kBarkFloats];
    v[0] *= s;
    v[1] = (v[1] - y0) * s;
    v[2] *= s;
  }
  for (TreeMesh::LeafPoint &p : out.LeafPoints) {
    p.Pos = Vec3(p.Pos.X * s, (p.Pos.Y - y0) * s, p.Pos.Z * s);
  }
  out.BoxMin = Vec3(mn.X * s, (mn.Y - y0) * s, mn.Z * s);
  out.BoxMax = Vec3(mx.X * s, (mx.Y - y0) * s, mx.Z * s);

  if (TrunkProfile_.empty()) { return; }
  out.FootRadius = TrunkProfile_[0].Y * s;
  out.BhdRadius = out.FootRadius;
  if (heightM <= 0.0f) { return; }
  const float yb = 1.3f / heightM;   /* breast height, in the mesh's own unit-height metric */
  for (size_t i = 1; i < TrunkProfile_.size(); ++i) {
    const float ya = (TrunkProfile_[i - 1].X - y0) * s, yc = (TrunkProfile_[i].X - y0) * s;
    if (yb > yc) { continue; }
    float u = yc > ya ? (yb - ya) / (yc - ya) : 0.0f;
    if (u < 0.0f) { u = 0.0f; }
    out.BhdRadius = (TrunkProfile_[i - 1].Y + (TrunkProfile_[i].Y - TrunkProfile_[i - 1].Y) * u) * s;
    return;
  }
  out.BhdRadius = TrunkProfile_.back().Y * s;
}

} // namespace outshine::World

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

TreeGrower::Tip TreeGrower::BranchFromFace(int fi, TreeVec3 dir, float radius, float step) {
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

  /* A REGULAR octagon at the collar AND at the first ring: equal segments, no distortion where the
   * branch leaves the trunk. */
  const int kb = kBranchSides;
  int inner[kBranchSides], ring[kBranchSides];
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
  Tip b = BranchFromFace(fi, dir, br, g.StepLen);
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

void TreeGrower::Grow(const TreeSpecies &species, TreeMesh &out) {
  const TreeSpecies::Growth &g = species.GrowthParams();

  Verts_.clear();
  Faces_.clear();
  Dead_.clear();
  Queue_.clear();
  out.Clear();
  Rng_ = TreeRandom(g.Seed);

  const float leafThreshold = g.TwigRadius * g.FoliageFactor;

  int k = g.TrunkSides;
  if (k > kMaxSides) { k = kMaxSides; }
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
      const int first = ExtrudeCap(t, oldDir, g.StepLen, nr, newRing);
      lastFirst = first;
      for (int i = 0; i < t.K; ++i) { t.Ring[i] = newRing[i]; }

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
        Tip b = BranchFromFace(fi, dir, t.Radius * 0.74f, g.StepLen);
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
  NormalizeToUnitHeight(out);
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

void TreeGrower::NormalizeToUnitHeight(TreeMesh &out) {
  if (out.BarkVerts.empty()) { return; }
  TreeVec3 mn = Vec3(1e30f, 1e30f, 1e30f), mx = Vec3(-1e30f, -1e30f, -1e30f);
  for (size_t i = 0; i < out.BarkVertexCount(); ++i) {
    const float *v = &out.BarkVerts[i * TreeMesh::kBarkFloats];
    mn = Vec3(std::fmin(mn.X, v[0]), std::fmin(mn.Y, v[1]), std::fmin(mn.Z, v[2]));
    mx = Vec3(std::fmax(mx.X, v[0]), std::fmax(mx.Y, v[1]), std::fmax(mx.Z, v[2]));
  }
  float h = mx.Y - mn.Y;
  if (h < 1e-6f) { h = 1.0f; }
  const float s = 1.0f / h;
  const float cx = (mn.X + mx.X) * 0.5f, cz = (mn.Z + mx.Z) * 0.5f, y0 = mn.Y;
  for (size_t i = 0; i < out.BarkVertexCount(); ++i) {
    float *v = &out.BarkVerts[i * TreeMesh::kBarkFloats];
    v[0] = (v[0] - cx) * s;
    v[1] = (v[1] - y0) * s;
    v[2] = (v[2] - cz) * s;
  }
  for (TreeMesh::LeafPoint &p : out.LeafPoints) {
    p.Pos = Vec3((p.Pos.X - cx) * s, (p.Pos.Y - y0) * s, (p.Pos.Z - cz) * s);
  }
  out.BoxMin = Vec3((mn.X - cx) * s, 0.0f, (mn.Z - cz) * s);
  out.BoxMax = Vec3((mx.X - cx) * s, (mx.Y - y0) * s, (mx.Z - cz) * s);
}

} // namespace outshine::World

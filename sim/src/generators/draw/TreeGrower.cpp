#include "TreeGrower.h"

#include <cmath>

namespace outshine::Generators {

namespace {

constexpr float kTau = 6.2831853f;
constexpr float kGolden = 2.39996323f; /* the divergence angle, in radians — phyllotaxis along a shoot */
constexpr float kDeg = 0.01745f;

/* [SET] How far past the silhouette a shoot may drift before it is ended rather than bent. A hard
 * stop AT the surface would end every shoot the wander pushes one step too far and leave the crown
 * edge ragged; 10 % is under one step of the coarsest declared step length. */
constexpr float kEscapeStop = 1.10f;
/* [SET] How hard a shoot outside the silhouette is steered back. A full turn onto the inward normal
 * makes the crown edge read as a moulded surface; this is a lean, not a wall. */
constexpr float kBendBack = 0.55f;
/* A shoot with fewer steps than this is not a branch — it is a stub the collar alone would show. */
constexpr int kMinBranchSteps = 3;

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

void TreeGrower::CapRing(const Tip &t, RingCap cap) {
  if (t.K == 0) { return; }
  float apex = 0.0f;
  switch (cap) {
    case RingCap::Base: apex = -0.6f; break;
    case RingCap::Point: apex = 2.4f; break;
    case RingCap::Cut: apex = 0.0f; break;
    /* A wind break is splinters, not a plane: the apex stands proud of a ring whose own vertices
     * are pulled forward at random, so the fracture is jagged from every side. */
    case RingCap::Broken: apex = 1.1f; break;
  }
  const int ci = AddVert(t.Pos + t.Dir * (t.Radius * apex));
  for (int k = 0; k < t.K; ++k) {
    if (cap == RingCap::Broken) {
      Verts_[(size_t)t.Ring[k]] =
          Verts_[(size_t)t.Ring[k]] + t.Dir * (t.Radius * Rng_.Unit() * 1.4f);
    }
    if (cap == RingCap::Base) {
      AddFace(ci, t.Ring[(k + 1) % t.K], t.Ring[k], -1);
    } else {
      AddFace(ci, t.Ring[k], t.Ring[(k + 1) % t.K], -1);
    }
  }
}

TreeVec3 TreeGrower::Inward(TreeVec3 p) const {
  if (Form_.Envelope == CrownEnvelope::Cut) {
    const float w = CrownHalfWidth_ > 0.0f ? std::fabs(p.Z) / CrownHalfWidth_ : 0.0f;
    const float l = HalfRunX_ > 0.0f ? std::fabs(p.X) / HalfRunX_ : 0.0f;
    const float h = CrownTopY_ > 0.0f ? p.Y / CrownTopY_ : 0.0f;
    if (h >= w && h >= l) { return Vec3(0, -1, 0); }
    if (w >= l) { return Vec3(0, 0, p.Z > 0.0f ? -1.0f : 1.0f); }
    return Vec3(p.X > 0.0f ? -1.0f : 1.0f, 0, 0);
  }
  const float r = std::sqrt(p.X * p.X + p.Z * p.Z);
  if (r < 1e-5f) { return Vec3(0, 0, 0); }
  return Vec3(-p.X / r, 0, -p.Z / r);
}

float TreeGrower::Escape(TreeVec3 p) const {
  if (Form_.Envelope == CrownEnvelope::Free || CrownHalfWidth_ <= 0.0f) { return 0.0f; }
  if (Form_.Envelope == CrownEnvelope::Cut) {
    const float w = std::fabs(p.Z) / CrownHalfWidth_;
    const float l = HalfRunX_ > 0.0f ? std::fabs(p.X) / HalfRunX_ : 0.0f;
    const float h = CrownTopY_ > 0.0f ? p.Y / CrownTopY_ : 0.0f;
    return std::fmax(std::fmax(w, l), h);
  }
  const float span = CrownTopY_ - CrownBaseY_;
  if (span <= 1e-5f) { return 0.0f; }
  const float allow =
      CrownHalfWidth_ * GrowthForm::Reach(Form_.Envelope, (p.Y - CrownBaseY_) / span);
  const float r = std::sqrt(p.X * p.X + p.Z * p.Z);
  if (allow <= 1e-5f) { return r > 1e-5f ? 1e3f : 0.0f; }
  return r / allow;
}

float TreeGrower::RoomInside(TreeVec3 from, TreeVec3 dir, float want) const {
  if (Form_.Envelope == CrownEnvelope::Free) { return want; }
  constexpr int kProbes = 8;
  for (int i = 1; i <= kProbes; ++i) {
    const float d = want * (float)i / (float)kProbes;
    if (Escape(from + dir * d) > 1.0f) { return want * (float)(i - 1) / (float)kProbes; }
  }
  return want;
}

TreeGrower::RingCap TreeGrower::LeaderEnd() const {
  switch (Form_.Arch) {
    case Architecture::Snag:
    case Architecture::FallenLog: return RingCap::Broken;
    case Architecture::Stump: return RingCap::Cut;
    case Architecture::SingleStemTree:
    case Architecture::MultiStemTree:
    case Architecture::MultiStemShrub:
    case Architecture::Bush:
    case Architecture::Hedge: return RingCap::Point;
  }
}

void TreeGrower::SetCrown(const TreeSpecies &species, float growHeight) {
  const float h = species.HeightM();
  CrownTopY_ = growHeight;
  CrownBaseY_ = Form_.BoleFrac * growHeight;
  /* THE DECLARED SPREAD IS THE ENVELOPE'S SCALE. `spread_m` was a number nothing read; it is now
   * the one that decides how wide the grown thing may get, in the same ratio to the height the
   * mesh is normalised by. */
  CrownHalfWidth_ = h > 0.0f ? 0.5f * species.SpreadM() / h * growHeight : 0.0f;
  HalfRunX_ = h > 0.0f ? 0.5f * Form_.RunM / h * growHeight : 0.0f;
}

/* THE BASE PLAN. One axis or many, and where they leave the ground: a stool's stems emerge within a
 * disc and lean out, a hedge's stand in a row along its own run, a tree's is the origin. The pipe
 * model (da Vinci's rule) divides the declared stem section among them, so a five-stemmed hazel is
 * one hazel and not five. */
void TreeGrower::SeedLeaders(const TreeSpecies::Growth &g, int bareSteps) {
  const int n = Form_.Leaders < 1 ? 1 : Form_.Leaders;
  const float radius = g.BaseRadius / std::sqrt((float)n);
  const float splay = Form_.LeaderSplayDeg * kDeg;
  const float stool = n > 1 && Form_.Arch != Architecture::Hedge ? 0.18f * CrownHalfWidth_ : 0.0f;
  int steps = g.TrunkSteps;
  if (Form_.BreakFrac > 0.0f) { steps = (int)(Form_.BreakFrac * (float)g.TrunkSteps + 0.5f); }
  if (steps < 1) { steps = 1; }

  for (int i = 0; i < n; ++i) {
    const float roll = kGolden * (float)i + Rng_.Signed() * 0.35f;
    const float lean = splay * (0.55f + 0.45f * Rng_.Unit());
    Tip t;
    t.Dir = Form_.Lying(Form_.Arch)
                ? Normalize(Vec3(std::cos(lean), std::sin(lean) * 0.35f, 0))
                : Normalize(Vec3(std::sin(lean) * std::cos(roll), std::cos(lean),
                                 std::sin(lean) * std::sin(roll)));
    t.Up = Vec3(0, 0, 1);
    if (Form_.Arch == Architecture::Hedge) {
      /* Planted at a spacing and grown at none: an even row reads as a plantation from every angle
       * a hedge is seen from. */
      const float u = ((float)i + 0.5f + 0.34f * Rng_.Signed()) / (float)n;
      t.Pos = Vec3(HalfRunX_ * (2.0f * u - 1.0f) * 0.94f,
                   0.0f, CrownHalfWidth_ * 0.5f * Rng_.Signed());
    } else {
      t.Pos = Vec3(stool * std::cos(roll), 0.0f, stool * std::sin(roll));
    }
    t.Radius = radius;
    t.Step = g.StepLen;
    t.Order = 0;
    t.Steps = steps;
    t.Bare = bareSteps;
    t.Leader = i;
    t.Roll = roll;
    t.Foliate = Form_.Foliate;
    t.K = SidesFor(radius, g.TrunkSides);
    TreeVec3 nf, bf;
    FrameFrom(t.Dir, t.Up, nf, bf);
    for (int k = 0; k < t.K; ++k) {
      const float ang = kTau * (float)k / (float)t.K;
      t.Ring[k] = AddVert(t.Pos + (nf * std::cos(ang) + bf * std::sin(ang)) * t.Radius);
    }
    if (i == 0) { TrunkProfile_.push_back(Vec3(t.Pos.Y, t.Radius, 0.0f)); }
    CapRing(t, RingCap::Base);
    Queue_.push_back(t);
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

/* A SHOOT WITHOUT A TUBE STILL BRANCHES. Returning here on `t.K == 0` made the crown's shape depend
 * on the detail knob: every rank coarse enough to drop a shoot's tube lost that shoot's whole
 * sub-tree and with it its leaf points. The side face is only where a branch is ANCHORED — without
 * one the tip's own frame gives the same direction. */
void TreeGrower::SpawnLateral(const Tip &t, const TreeSpecies::Growth &g, int first, float roll,
                              int parentStep) {
  int fi = -1;
  TreeVec3 fn;
  if (t.K > 0 && first >= 0) {
    int k0 = (int)(roll / kTau * (float)t.K) % t.K;
    if (k0 < 0) { k0 += t.K; }
    fi = first + k0;
    if (Dead_[(size_t)fi]) { return; }
    fn = FaceNormal(fi);
  } else {
    TreeVec3 n, b;
    FrameFrom(t.Dir, t.Up, n, b);
    fn = n * std::cos(roll) + b * std::sin(roll);
  }
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
  const bool foliate =
      t.Foliate && (t.Order != 0 || parentStep >= (int)(Form_.BoleFrac * (float)t.Steps));
  SpawnShoot(t, Shoot{fi, dir, fn, br, foliate}, g);
}

/* PRUNED AT SPAWN, not after: a shoot is given the run that stays inside the declared silhouette,
 * and one with no run at all is not grown. Cutting a finished crown back would leave the severed
 * stubs; this never grows them. */
void TreeGrower::SpawnShoot(const Tip &parent, const Shoot &shoot, const TreeSpecies::Growth &g) {
  const TreeVec3 from = shoot.Face >= 0 ? FaceCentroid(shoot.Face) : parent.Pos;
  int steps = (int)((float)parent.Steps * g.OrderLen);
  if (steps < kMinBranchSteps) { steps = kMinBranchSteps; }
  const float len = RoomInside(from, shoot.Dir, (float)steps * parent.Step);
  /* A shoot shorter than its own diameter is a collar and not a branch. */
  if (len < 2.0f * shoot.Radius) { return; }
  const float step = len / (float)steps;

  Tip b;
  if (shoot.Face < 0 || 2.0f * shoot.Radius <= PixelGrow_) {
    b.K = 0;
    b.Dir = shoot.Dir;
    b.Up = shoot.Up;
    b.Pos = from;
    b.Radius = shoot.Radius;
  } else {
    b = BranchFromFace(shoot.Face, shoot.Dir, shoot.Radius, step,
                       SidesFor(shoot.Radius, kBranchSides));
  }
  b.Order = parent.Order + 1;
  b.Steps = steps;
  b.Step = step;
  b.Bare = 1;
  b.Leader = parent.Leader;
  b.Foliate = shoot.Foliate;
  Queue_.push_back(b);
}

/* THE STEM IS SOLVED, NOT DECLARED. `base_radius` lives in grower units and the mesh is normalised by
 * a height the branches decide, so the same 0.08 came out as 70 cm on a beech and 80 cm on an elm —
 * measured. The species declares the number forestry measures (`dbh_cm`) and the whole radius cascade
 * is scaled to hit it; scaling base, twig and min together keeps every branch's termination and every
 * leaf point exactly where they were, so only the thickness moves. */
void TreeGrower::Grow(const TreeSpecies &species, TreeMesh &out, float pixelHeightFrac) {
  TreeSpecies::Growth g = species.GrowthParams();
  Form_ = species.Form();
  const float h = species.HeightM();
  const float targetR = species.DbhM() * 0.5f;

  /* The rule arrives in tree HEIGHTS and the grower works in its own units, so the first pass runs on
   * an ESTIMATE of the height and the second on the one it measured. `trunk_steps * step_len` alone
   * will not do — the crown reaches well past the leader's own run (beech 6.80 against 4.16, oak
   * 4,54 against 2,24, both measured) — so the estimate carries that factor and the refinement
   * removes it. */
  const float estimate = 1.6f * (float)g.TrunkSteps * g.StepLen;
  PixelGrow_ = pixelHeightFrac > 0.0f ? pixelHeightFrac * estimate : 0.0f;
  SetCrown(species, estimate);
  Passes_ = 1;
  GrowOnce(g, h, out);
  /* THE SECOND PASS IS NOT OPTIONAL ANY MORE: the envelope is scaled by the height the mesh comes
   * out at, so the first pass shapes a crown against an estimate and only the second against the
   * measurement. */
  PixelGrow_ = pixelHeightFrac > 0.0f ? pixelHeightFrac * GrowHeight_ : 0.0f;
  SetCrown(species, GrowHeight_);
  GrowOnce(g, h, out);
  Passes_++;
  /* THE VERTEX COUNT IS A BUDGET AND IT IS SOLVED, NOT CUT. Truncating the queue at a ceiling drops
   * whichever tips the breadth-first order reached last — the outermost shoots, the ones the crown
   * is made of. Coarsening the pixel drops TUBES instead, and a shoot without a tube still grows,
   * still branches and still carries its leaves. */
  for (int i = 0; i < 4 && (int)Verts_.size() > kVertexBudget; ++i) {
    const float over = std::sqrt((float)Verts_.size() / (float)kVertexBudget);
    PixelGrow_ = (PixelGrow_ > 0.0f ? PixelGrow_ : 2.0f * g.MinRadius) * over;
    GrowOnce(g, h, out);
    Passes_++;
  }
  DbhErrorRel_ = 0.0f;
  if (targetR <= 0.0f || h <= 0.0f) { return; }

  for (int i = 0; i < 4; ++i) {
    const float haveR = out.DbhRadius * h;
    DbhErrorRel_ = haveR > 0.0f ? (haveR - targetR) / targetR : 0.0f;
    if (haveR <= 0.0f || std::fabs(DbhErrorRel_) < 0.005f) { break; }
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
  const int bareSteps = (int)(Form_.BoleFrac * (float)g.TrunkSteps + 0.5f);
  SeedLeaders(g, bareSteps);

  for (size_t head = 0; head < Queue_.size(); ++head) {
    Tip t = Queue_[head];
    int newRing[kMaxSides];
    int lastFirst = -1;
    float leafRoll = t.Roll;
    for (int s = 0; s < t.Steps; ++s) {
      if ((int)Verts_.size() > kVertexBudget) { break; }
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
        CapRing(t, RingCap::Point);
        t.K = 0;
        lastFirst = -1;
      }
      int first = -1;
      if (t.K > 0) {
        first = ExtrudeCap(t, oldDir, t.Step, nr, newRing);
        lastFirst = first;
        for (int i = 0; i < t.K; ++i) { t.Ring[i] = newRing[i]; }
      } else {
        t.Pos = t.Pos + t.Dir * t.Step;
        t.Radius = nr;
      }
      if (t.Order == 0 && t.Leader == 0) { TrunkProfile_.push_back(Vec3(t.Pos.Y, t.Radius, 0.0f)); }

      /* THE SILHOUETTE IS A WALL FOR A BRANCH AND A LEAN FOR A LEADER. A leader that could be cut
       * short would shorten the very height the envelope is scaled by, and the next pass would take
       * the shorter one — the plant would walk itself into the ground over two passes. */
      const float escaped = Escape(t.Pos);
      if (escaped > 1.0f) {
        const float pull = std::fmin(1.0f, (escaped - 1.0f) * 4.0f) * kBendBack;
        t.Dir = Normalize(t.Dir + Inward(t.Pos) * pull);
        if (t.Order > 0 && escaped > kEscapeStop) { break; }
      }

      const bool leaderOk = (t.Order != 0) || (s >= (int)(Form_.BoleFrac * (float)t.Steps));
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
          if (((s - bareSteps) % g.WhorlSpacing) == 0) {
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
    if (g.TerminalFork && t.Radius > g.TwigRadius && t.Order <= g.MaxOrder &&
        (int)Verts_.size() < kSpawnCeiling) {
      const int km[2] = {0, t.K / 2};
      TreeVec3 fnA, fnB;
      FrameFrom(t.Dir, t.Up, fnA, fnB);
      for (int j = 0; j < 2; ++j) {
        const int fi = (lastFirst >= 0 && t.K > 0) ? lastFirst + km[j] : -1;
        if (fi >= 0 && Dead_[(size_t)fi]) { continue; }
        const TreeVec3 fn = fi >= 0 ? FaceNormal(fi) : (j == 0 ? fnA : fnA * -1.0f);
        const TreeVec3 dir = Normalize(t.Dir + fn * 0.55f);
        SpawnShoot(t, Shoot{fi, dir, fn, t.Radius * 0.74f, t.Foliate}, g);
      }
    }
    CapRing(t, t.Order == 0 ? LeaderEnd() : RingCap::Point);
    if (t.Foliate && ((t.Order >= 1) || (g.FoliageOnLeader && t.Radius < leafThreshold * 1.7f)) &&
        t.Radius < leafThreshold * 1.7f) {
      EmitLeafPoints(out, t.Pos, t.Dir, t.Up, t.Radius, 4, leafRoll + 1.1f);
    }
  }

  Export(out);
  NormalizeToUnitHeight(out, heightM);
}

void TreeGrower::Export(TreeMesh &out) {
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

/* THE ORIGIN IS THE TRUNK FOOT, and the crown's bounding box has no say in it. Centring x/z on the box
 * put the stem up to 5.65 m (pine, measured) off the point the scatter placed the tree at, and yaw
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
  /* THE CROWN IS THE BOX, NOT THE BARK. Bark disappears from the finest shoots as the detail knob
   * coarsens; the leaf points do not, so measuring the box over both is what makes one tree the same
   * size at every rank. */
  for (const TreeMesh::LeafPoint &p : out.LeafPoints) {
    mn = Vec3(std::fmin(mn.X, p.Pos.X), std::fmin(mn.Y, p.Pos.Y), std::fmin(mn.Z, p.Pos.Z));
    mx = Vec3(std::fmax(mx.X, p.Pos.X), std::fmax(mx.Y, p.Pos.Y), std::fmax(mx.Z, p.Pos.Z));
  }
  /* Y = 0 IS THE TRUNK FOOT, not the lowest vertex, and `height_m` is measured from there — which is
   * what a stand height is. Taking the mesh minimum put a Trauerweide's foot 6,87 m and a Fichte's
   * 3,67 m above the ground (measured), because both hang branches below their own base. A branch
   * below zero belongs below the terrain; that is where it grows. */
  /* A LYING BODY IS MEASURED ALONG ITS OWN AXIS. `height_m` is the standing extent of a standing
   * plant and the LENGTH of a fallen one; normalising a log by its vertical extent would make the
   * instance scale its DIAMETER to the declared metres. */
  const bool lying = GrowthForm::Lying(Form_.Arch);
  const float y0 = lying ? mn.Y
                         : (TrunkProfile_.empty() ? mn.Y
                                                  : TrunkProfile_[0].X - 0.6f * TrunkProfile_[0].Y);
  float h = lying ? std::fmax(mx.X - mn.X, mx.Z - mn.Z) : mx.Y - y0;
  if (h < 1e-6f) { h = 1.0f; }
  GrowHeight_ = h;
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
  out.DbhRadius = out.FootRadius;
  if (heightM <= 0.0f) { return; }
  const float yb = 1.3f / heightM;   /* breast height, in the mesh's own unit-height metric */
  for (size_t i = 1; i < TrunkProfile_.size(); ++i) {
    const float ya = (TrunkProfile_[i - 1].X - y0) * s, yc = (TrunkProfile_[i].X - y0) * s;
    if (yb > yc) { continue; }
    float u = yc > ya ? (yb - ya) / (yc - ya) : 0.0f;
    if (u < 0.0f) { u = 0.0f; }
    out.DbhRadius = (TrunkProfile_[i - 1].Y + (TrunkProfile_[i].Y - TrunkProfile_[i - 1].Y) * u) * s;
    return;
  }
  out.DbhRadius = TrunkProfile_.back().Y * s;
}

} // namespace outshine::Generators

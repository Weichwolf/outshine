#include "TreeGrower.h"

#include <numbers>
#include <cmath>

namespace outshine::Generators {

namespace {

constexpr float kWhorlJitterRad = 0.25f;
constexpr float kSpiralJitterRad = 0.4f;

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
constexpr float kGolden = 2.0f * std::numbers::pi_v<float> * (2.0f - std::numbers::phi_v<float>);
constexpr float kDeg = std::numbers::pi_v<float> / 180.0f;

constexpr float kEscapeStop = 1.10f;

constexpr float kBendBack = 0.55f;

constexpr int kMinBranchSteps = 3;

constexpr float kCapReach = 2.4f;

TreeVec3 RadialAt(TreeVec3 dir, TreeVec3 up, float roll) {
  TreeVec3 n, b;
  FrameFrom(dir, up, n, b);
  return n * std::cos(roll) + b * std::sin(roll);
}

} // namespace

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
    const float d = want * static_cast<float>(i) / static_cast<float>(kProbes);
    if (Escape(from + dir * d) > 1.0f) {
      return want * static_cast<float>(i - 1) / static_cast<float>(kProbes);
    }
  }
  return want;
}

RingCap TreeGrower::LeaderEnd() const {
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

  CrownHalfWidth_ = h > 0.0f ? 0.5f * species.SpreadM() / h * growHeight : 0.0f;
  HalfRunX_ = h > 0.0f ? 0.5f * Form_.RunM / h * growHeight : 0.0f;
}

int TreeGrower::AddNode(int shoot, TreeVec3 pos, TreeVec3 dir, TreeVec3 up, float radius) {
  Plant_->Nodes.push_back(TreeSkeleton::Node{.Pos = pos, .Dir = dir, .Up = up, .Radius = radius});
  Plant_->Shoots[static_cast<size_t>(shoot)].Count++;
  return static_cast<int>(Plant_->Nodes.size()) - 1;
}

void TreeGrower::SeedLeaders(const TreeSpecies::Growth &g, int bareSteps) {
  const int n = Form_.Leaders < 1 ? 1 : Form_.Leaders;
  const float radius = g.BaseRadius / std::sqrt(static_cast<float>(n));
  const float splay = Form_.LeaderSplayDeg * kDeg;
  constexpr float kStoolOfCrown = 0.18f;
  const float stool =
      n > 1 && Form_.Arch != Architecture::Hedge ? kStoolOfCrown * CrownHalfWidth_ : 0.0f;
  int steps = g.TrunkSteps;
  if (Form_.BreakFrac > 0.0f) {
    steps = static_cast<int>(Form_.BreakFrac * static_cast<float>(g.TrunkSteps) + 0.5f);
  }
  if (steps < 1) { steps = 1; }

  for (int i = 0; i < n; ++i) {
    constexpr float kRollJitterRad = 0.35f;
    constexpr float kLeanLeast = 0.55f;
    const float roll = kGolden * static_cast<float>(i) + Rng_.Signed() * kRollJitterRad;
    const float lean = splay * (kLeanLeast + (1.0f - kLeanLeast) * Rng_.Unit());
    Tip t;
    t.Dir = Form_.Lying(Form_.Arch) ? Normalize(Vec3(std::cos(lean), std::sin(lean) * 0.35f, 0))
                                    : Normalize(Vec3(std::sin(lean) * std::cos(roll),
                                                     std::cos(lean),
                                                     std::sin(lean) * std::sin(roll)));
    t.Up = Vec3(0, 0, 1);
    if (Form_.Arch == Architecture::Hedge) {
      constexpr float kSlotJitter = 0.34f;
      constexpr float kRunInset = 0.94f;
      constexpr float kCrossOfCrown = 0.5f;
      const float u =
          (static_cast<float>(i) + 0.5f + kSlotJitter * Rng_.Signed()) / static_cast<float>(n);
      t.Pos = Vec3(HalfRunX_ * (2.0f * u - 1.0f) * kRunInset,
                   0.0f,
                   CrownHalfWidth_ * kCrossOfCrown * Rng_.Signed());
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
    t.Shoot = static_cast<int>(Plant_->Shoots.size());
    TreeSkeleton::Shoot s;
    s.Sides = g.TrunkSides;
    s.End = LeaderEnd();
    Plant_->Shoots.push_back(s);
    Queue_.push_back(t);
  }
}

void TreeGrower::EmitLeafPoints(
    TreeVec3 pos, TreeVec3 dir, TreeVec3 up, float radius, int count, float roll) {
  TreeVec3 n, b;
  FrameFrom(dir, up, n, b);
  if (count < 1) { count = 1; }
  constexpr float kAzimuthJitterRad = 0.30f;
  constexpr float kSeatOfRadius = 0.95f;
  constexpr float kForwardTilt = 0.40f;
  for (int i = 0; i < count; ++i) {
    const float az = roll + kTau * (static_cast<float>(i) / static_cast<float>(count)) +
                     Rng_.Signed() * kAzimuthJitterRad;
    const TreeVec3 radial = n * std::cos(az) + b * std::sin(az);
    Plant_->LeafPoints.push_back(LeafPoint{.Pos = pos + radial * (radius * kSeatOfRadius),
                                           .Dir = Normalize(radial + dir * kForwardTilt)});
  }
}

void TreeGrower::SpawnLateral(
    const Tip &t, const TreeSpecies::Growth &g, int node, float roll, int parentStep) {
  const TreeVec3 fn = RadialAt(t.Dir, t.Up, roll);
  const float a = (g.BranchAngle + Rng_.Signed() * g.BranchAngleVar) * kDeg;
  const TreeVec3 dir = Normalize(t.Dir * std::cos(a) + fn * std::sin(a));
  const float br = t.Radius * g.OrderRadius;
  if (br <= g.MinRadius) { return; }

  if (g.ShadePrune > 0.0f && t.Order >= 2) {
    const float rl = std::sqrt(t.Pos.X * t.Pos.X + t.Pos.Z * t.Pos.Z);
    if (rl > 1e-4f) {
      const float dOut = (dir.X * t.Pos.X + dir.Z * t.Pos.Z) / rl;
      constexpr float kInwardEnough = -0.05f;
      if (dOut < kInwardEnough && Rng_.Unit() < g.ShadePrune) { return; }
    }
  }
  const bool foliate =
      t.Foliate && (t.Order != 0 ||
                    parentStep >= static_cast<int>(Form_.BoleFrac * static_cast<float>(t.Steps)));
  SpawnShoot(
      t,
      Request{
          .ParentNode = node, .Roll = roll, .Dir = dir, .Up = fn, .Radius = br, .Foliate = foliate},
      g);
}

void TreeGrower::SpawnShoot(const Tip &parent,
                            const Request &request,
                            const TreeSpecies::Growth &g) {
  const std::vector<TreeSkeleton::Node> &nodes = Plant_->Nodes;
  const TreeSkeleton::Node &upper = nodes[static_cast<size_t>(request.ParentNode)];
  const TreeSkeleton::Node &lower = nodes[static_cast<size_t>(request.ParentNode - 1)];
  const TreeVec3 from =
      (upper.Pos + lower.Pos) * 0.5f +
      RadialAt(upper.Dir, upper.Up, request.Roll) * (0.5f * (upper.Radius + lower.Radius));

  int steps = static_cast<int>(static_cast<float>(parent.Steps) * g.OrderLen);
  if (steps < kMinBranchSteps) { steps = kMinBranchSteps; }
  const float len = RoomInside(from, request.Dir, static_cast<float>(steps) * parent.Step);

  if (len < 2.0f * request.Radius) { return; }

  Tip b;
  b.Dir = request.Dir;
  b.Up = request.Up;
  b.Pos = from;
  b.Radius = request.Radius;
  b.Order = parent.Order + 1;
  b.Steps = steps;
  b.Step = len / static_cast<float>(steps);
  b.Bare = 1;
  b.Leader = parent.Leader;
  b.Foliate = request.Foliate;
  b.Shoot = static_cast<int>(Plant_->Shoots.size());

  TreeSkeleton::Shoot s;
  s.Parent = parent.Shoot;
  s.ParentNode = request.ParentNode;
  s.Roll = request.Roll;
  s.Sides = kBranchSides;
  s.End = RingCap::Point;
  Plant_->Shoots.push_back(s);
  Queue_.push_back(b);
}

void TreeGrower::GrowOnce(const TreeSpecies::Growth &g, float heightM) {
  Plant_->Clear();
  Queue_.clear();
  TrunkProfile_.clear();
  Rng_ = TreeRandom(g.Seed);

  const float leafThreshold = g.TwigRadius * g.FoliageFactor;
  const int bareSteps = static_cast<int>(Form_.BoleFrac * static_cast<float>(g.TrunkSteps) + 0.5f);
  SeedLeaders(g, bareSteps);

  for (size_t head = 0; head < Queue_.size(); ++head) {
    Tip t = Queue_[head];
    Plant_->Shoots[static_cast<size_t>(t.Shoot)].First = static_cast<int>(Plant_->Nodes.size());
    float leafRoll = t.Roll;

    {
      TreeVec3 n, b;
      FrameFrom(t.Dir, t.Up, n, b);
      t.Up = n;
    }
    AddNode(t.Shoot, t.Pos, t.Dir, t.Up, t.Radius);
    if (Plant_->Shoots[static_cast<size_t>(t.Shoot)].Parent >= 0) {
      t.Pos = t.Pos + t.Dir * t.Step;
      AddNode(t.Shoot, t.Pos, t.Dir, t.Up, t.Radius);
    } else if (t.Leader == 0) {
      TrunkProfile_.push_back(Vec3(t.Pos.Y, t.Radius, 0.0f));
    }

    int last = static_cast<int>(Plant_->Nodes.size()) - 1;
    for (int s = 0; s < t.Steps; ++s) {
      if (static_cast<int>(Plant_->Nodes.size()) >= kMaxNodes) { break; }
      const TreeVec3 oldDir = t.Dir;

      TreeVec3 nf, bf;
      FrameFrom(t.Dir, t.Up, nf, bf);
      const float wr = g.Wander * kDeg;
      const float ub = (t.Order == 0) ? g.LeaderBias : g.BranchUpBias;
      TreeVec3 want = t.Dir;
      want = want + nf * (Rng_.Signed() * wr);
      want = want + bf * (Rng_.Signed() * wr);
      want = want + Vec3(0, 1, 0) * ub;
      t.Dir = Normalize(want);

      const TreeVec3 nPos = t.Pos + t.Dir * t.Step;
      TreeVec3 up = RmfDouble(t.Pos, nPos, oldDir, t.Dir, t.Up);
      up = Normalize(up - t.Dir * Dot(up, t.Dir));
      t.Up = up;
      t.Pos = nPos;
      t.Radius = t.Radius * g.Taper;
      last = AddNode(t.Shoot, t.Pos, t.Dir, t.Up, t.Radius);
      if (t.Order == 0 && t.Leader == 0) { TrunkProfile_.push_back(Vec3(t.Pos.Y, t.Radius, 0.0f)); }

      const float escaped = Escape(t.Pos);
      if (escaped > 1.0f) {
        constexpr float kEscapeToFull = 0.25f;
        const float pull = std::fmin(1.0f, (escaped - 1.0f) / kEscapeToFull) * kBendBack;
        t.Dir = Normalize(t.Dir + Inward(t.Pos) * pull);
        if (t.Order > 0 && escaped > kEscapeStop) { break; }
      }

      const bool leaderOk =
          (t.Order != 0) || (s >= static_cast<int>(Form_.BoleFrac * static_cast<float>(t.Steps)));
      const bool foliated = t.Foliate && leaderOk &&
                            ((t.Order >= 1) || (g.FoliageOnLeader && t.Radius < leafThreshold));
      if (foliated && t.Radius < leafThreshold) {
        leafRoll += kGolden;
        EmitLeafPoints(t.Pos, t.Dir, t.Up, t.Radius, 3, leafRoll);
      }

      if (t.Bare > 0) {
        t.Bare--;
      } else if (t.Order < g.MaxOrder && static_cast<int>(Plant_->Nodes.size()) < kMaxNodes) {
        if (g.WhorlCount > 0 && t.Order == 0) {
          if (((s - bareSteps) % g.WhorlSpacing) == 0) {
            for (int wb = 0; wb < g.WhorlCount; ++wb) {
              SpawnLateral(t,
                           g,
                           last,
                           static_cast<float>(wb) * kTau / static_cast<float>(g.WhorlCount) +
                               Rng_.Signed() * kWhorlJitterRad,
                           s);
            }
          }
        } else if (Rng_.Unit() < g.BranchChance) {
          t.Roll += kGolden + Rng_.Signed() * kSpiralJitterRad;
          SpawnLateral(t, g, last, t.Roll, s);
        }
      }
      if (t.Radius < g.MinRadius) { break; }
    }

    if (g.TerminalFork && t.Radius > g.TwigRadius && t.Order <= g.MaxOrder &&
        static_cast<int>(Plant_->Nodes.size()) < kMaxNodes &&
        last > Plant_->Shoots[static_cast<size_t>(t.Shoot)].First) {
      for (int j = 0; j < 2; ++j) {
        const float roll = j == 0 ? 0.0f : kTau * 0.5f;
        const TreeVec3 fn = RadialAt(t.Dir, t.Up, roll);
        const TreeVec3 dir = Normalize(t.Dir + fn * 0.55f);
        SpawnShoot(t,
                   Request{.ParentNode = last,
                           .Roll = roll,
                           .Dir = dir,
                           .Up = fn,
                           .Radius = t.Radius * 0.74f,
                           .Foliate = t.Foliate},
                   g);
      }
    }
    if (t.Foliate && ((t.Order >= 1) || (g.FoliageOnLeader && t.Radius < leafThreshold * 1.7f)) &&
        t.Radius < leafThreshold * 1.7f) {
      EmitLeafPoints(t.Pos, t.Dir, t.Up, t.Radius, 4, leafRoll + 1.1f);
    }
  }

  MeasureReach();
  NormalizeToUnitHeight(heightM);
}

void TreeGrower::MeasureReach() {
  std::vector<TreeSkeleton::Shoot> &shoots = Plant_->Shoots;
  const std::vector<TreeSkeleton::Node> &nodes = Plant_->Nodes;
  for (TreeSkeleton::Shoot &s : shoots) {
    s.Reach = 0.0f;
    if (s.Count <= 0) { continue; }
    const TreeVec3 anchor = nodes[static_cast<size_t>(s.First)].Pos;
    for (int i = s.First; i < s.First + s.Count; ++i) {
      const TreeSkeleton::Node &n = nodes[static_cast<size_t>(i)];
      s.Reach = std::fmax(s.Reach, Length(n.Pos - anchor) + kCapReach * n.Radius);
    }
  }

  for (size_t i = shoots.size(); i-- > 0;) {
    const TreeSkeleton::Shoot &child = shoots[i];
    if (child.Parent < 0 || child.Count <= 0) { continue; }
    TreeSkeleton::Shoot &parent = shoots[static_cast<size_t>(child.Parent)];
    if (parent.Count <= 0) { continue; }
    const float apart = Length(nodes[static_cast<size_t>(child.First)].Pos -
                               nodes[static_cast<size_t>(parent.First)].Pos);
    parent.Reach = std::fmax(parent.Reach, apart + child.Reach);
  }
}

void TreeGrower::NormalizeToUnitHeight(float heightM) {
  if (Plant_->Nodes.empty()) { return; }
  TreeVec3 mn = Vec3(1e30f, 1e30f, 1e30f), mx = Vec3(-1e30f, -1e30f, -1e30f);
  auto cover = [&mn, &mx](TreeVec3 p, TreeVec3 half) {
    mn = Vec3(std::fmin(mn.X, p.X - half.X),
              std::fmin(mn.Y, p.Y - half.Y),
              std::fmin(mn.Z, p.Z - half.Z));
    mx = Vec3(std::fmax(mx.X, p.X + half.X),
              std::fmax(mx.Y, p.Y + half.Y),
              std::fmax(mx.Z, p.Z + half.Z));
  };

  for (const TreeSkeleton::Shoot &s : Plant_->Shoots) {
    for (int i = s.First; i < s.First + s.Count; ++i) {
      const TreeSkeleton::Node &n = Plant_->Nodes[static_cast<size_t>(i)];
      const TreeVec3 disc = Vec3(n.Radius * std::sqrt(std::fmax(0.0f, 1.0f - n.Dir.X * n.Dir.X)),
                                 n.Radius * std::sqrt(std::fmax(0.0f, 1.0f - n.Dir.Y * n.Dir.Y)),
                                 n.Radius * std::sqrt(std::fmax(0.0f, 1.0f - n.Dir.Z * n.Dir.Z)));
      cover(n.Pos, disc);
      if (i == s.First && s.Parent < 0) { cover(n.Pos - n.Dir * (0.6f * n.Radius), disc); }
      if (i + 1 == s.First + s.Count) {
        const float apex =
            s.End == RingCap::Point ? 2.4f : (s.End == RingCap::Broken ? 1.4f : 0.0f);
        cover(n.Pos + n.Dir * (apex * n.Radius), disc);
      }
    }
  }
  for (const LeafPoint &p : Plant_->LeafPoints) { cover(p.Pos, TreeVec3{}); }

  const bool lying = GrowthForm::Lying(Form_.Arch);
  const float y0 =
      lying ? mn.Y
            : (TrunkProfile_.empty() ? mn.Y : TrunkProfile_[0].X - 0.6f * TrunkProfile_[0].Y);
  float h = lying ? std::fmax(mx.X - mn.X, mx.Z - mn.Z) : mx.Y - y0;
  if (h < 1e-6f) { h = 1.0f; }
  GrowHeight_ = h;
  const float s = 1.0f / h;
  for (TreeSkeleton::Node &n : Plant_->Nodes) {
    n.Pos = Vec3(n.Pos.X * s, (n.Pos.Y - y0) * s, n.Pos.Z * s);
    n.Radius *= s;
  }
  for (LeafPoint &p : Plant_->LeafPoints) {
    p.Pos = Vec3(p.Pos.X * s, (p.Pos.Y - y0) * s, p.Pos.Z * s);
  }
  for (TreeSkeleton::Shoot &shoot : Plant_->Shoots) { shoot.Reach *= s; }
  Plant_->BoxMin = Vec3(mn.X * s, (mn.Y - y0) * s, mn.Z * s);
  Plant_->BoxMax = Vec3(mx.X * s, (mx.Y - y0) * s, mx.Z * s);

  if (TrunkProfile_.empty()) { return; }
  Plant_->FootRadius = TrunkProfile_[0].Y * s;
  Plant_->DbhRadius = Plant_->FootRadius;
  if (heightM <= 0.0f) { return; }
  const float yb = 1.3f / heightM;
  for (size_t i = 1; i < TrunkProfile_.size(); ++i) {
    const float ya = (TrunkProfile_[i - 1].X - y0) * s, yc = (TrunkProfile_[i].X - y0) * s;
    if (yb > yc) { continue; }
    float u = yc > ya ? (yb - ya) / (yc - ya) : 0.0f;
    if (u < 0.0f) { u = 0.0f; }
    Plant_->DbhRadius =
        (TrunkProfile_[i - 1].Y + (TrunkProfile_[i].Y - TrunkProfile_[i - 1].Y) * u) * s;
    return;
  }
  Plant_->DbhRadius = TrunkProfile_.back().Y * s;
}

void TreeGrower::Grow(const TreeSpecies &species, TreeSkeleton &out) {
  Plant_ = &out;
  TreeSpecies::Growth g = species.GrowthParams();
  Form_ = species.Form();
  const float h = species.HeightM();
  const float targetR = species.DbhM() * 0.5f;
  out.Seed = g.Seed;

  SetCrown(species, 1.6f * static_cast<float>(g.TrunkSteps) * g.StepLen);
  Passes_ = 1;
  GrowOnce(g, h);

  SetCrown(species, GrowHeight_);
  GrowOnce(g, h);
  Passes_++;

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
    GrowOnce(g, h);
    Passes_++;
  }
}

} // namespace outshine::Generators

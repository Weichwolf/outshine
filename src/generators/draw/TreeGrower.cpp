#include "Units.h"
#include "TreeFrame.h"
#include "TreeGrower.h"

#include <algorithm>
#include <cstddef>
#include <numbers>
#include <cmath>
#include <vector>

namespace outshine::Generators {

namespace {

constexpr float kLeastRadiusM = 1e-5f;
constexpr float kLeastRunM = 1e-4f;
constexpr float kFarBeyondAllowance = 1e3f;
constexpr float kTipTaper = 0.74f;
constexpr float kLeafRadiusFactor = 1.7f;
constexpr int kLeafPointsPerWhorl = 4;
constexpr float kLyingRise = 0.35f;

constexpr float kWhorlJitterRad = 0.25f;
constexpr float kSpiralJitterRad = 0.4f;

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
constexpr float kGolden = 2.0f * std::numbers::pi_v<float> * (2.0f - std::numbers::phi_v<float>);
constexpr float kDeg = static_cast<float>(kDeg2Rad);

constexpr float kEscapeStop = 1.10f;

constexpr float kBendBack = 0.55f;

constexpr int kMinBranchSteps = 3;

constexpr float kCapReach = 2.4f;

Vec3f RadialAt(Vec3f dir, Vec3f up, float roll) {
  Vec3f n;
  Vec3f b;
  FrameFrom(dir, up, n, b);
  return n * std::cos(roll) + b * std::sin(roll);
}

} // namespace

Vec3f TreeGrower::Inward(Vec3f p) const {
  if (Form_.Envelope == CrownEnvelope::Cut) {
    const float w = CrownHalfWidth_ > 0.0f ? std::fabs(p[2]) / CrownHalfWidth_ : 0.0f;
    const float l = HalfRunX_ > 0.0f ? std::fabs(p[0]) / HalfRunX_ : 0.0f;
    const float h = CrownTopY_ > 0.0f ? p[1] / CrownTopY_ : 0.0f;
    if (h >= w && h >= l) { return Vec3f{{0, -1, 0}}; }
    if (w >= l) { return Vec3f{{0, 0, p[2] > 0.0f ? -1.0f : 1.0f}}; }
    return Vec3f{{p[0] > 0.0f ? -1.0f : 1.0f, 0, 0}};
  }
  const float r = std::sqrt(p[0] * p[0] + p[2] * p[2]);
  if (r < kLeastRadiusM) { return Vec3f{{0, 0, 0}}; }
  return Vec3f{{-p[0] / r, 0, -p[2] / r}};
}

float TreeGrower::Escape(Vec3f p) const {
  if (Form_.Envelope == CrownEnvelope::Free || CrownHalfWidth_ <= 0.0f) { return 0.0f; }
  if (Form_.Envelope == CrownEnvelope::Cut) {
    const float w = std::fabs(p[2]) / CrownHalfWidth_;
    const float l = HalfRunX_ > 0.0f ? std::fabs(p[0]) / HalfRunX_ : 0.0f;
    const float h = CrownTopY_ > 0.0f ? p[1] / CrownTopY_ : 0.0f;
    return std::fmax(std::fmax(w, l), h);
  }
  const float span = CrownTopY_ - CrownBaseY_;
  if (span <= kLeastRadiusM) { return 0.0f; }
  const float allow =
      CrownHalfWidth_ * GrowthForm::Reach(Form_.Envelope, (p[1] - CrownBaseY_) / span);
  const float r = std::sqrt(p[0] * p[0] + p[2] * p[2]);
  if (allow <= kLeastRadiusM) { return r > kLeastRadiusM ? kFarBeyondAllowance : 0.0f; }
  return r / allow;
}

float TreeGrower::RoomInside(Vec3f from, Vec3f dir, float want) const {
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

int TreeGrower::AddNode(int shoot, Vec3f pos, Vec3f dir, Vec3f up, float radius) {
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
  steps = std::max(steps, 1);

  for (int i = 0; i < n; ++i) {
    constexpr float kRollJitterRad = 0.35f;
    constexpr float kLeanLeast = 0.55f;
    const float roll = kGolden * static_cast<float>(i) + Rng_.Signed() * kRollJitterRad;
    const float lean = splay * (kLeanLeast + (1.0f - kLeanLeast) * Rng_.Unit());
    Tip t;
    t.Dir = outshine::Generators::GrowthForm::Lying(Form_.Arch)
                ? DirectionOrUp(Vec3f{{std::cos(lean), std::sin(lean) * kLyingRise, 0}})
                : DirectionOrUp(Vec3f{{std::sin(lean) * std::cos(roll),
                                       std::cos(lean),
                                       std::sin(lean) * std::sin(roll)}});
    t.Up = Vec3f{{0, 0, 1}};
    if (Form_.Arch == Architecture::Hedge) {
      constexpr float kSlotJitter = 0.34f;
      constexpr float kRunInset = 0.94f;
      constexpr float kCrossOfCrown = 0.5f;
      const float u =
          (static_cast<float>(i) + 0.5f + kSlotJitter * Rng_.Signed()) / static_cast<float>(n);
      t.Pos = Vec3f{{HalfRunX_ * (2.0f * u - 1.0f) * kRunInset,
                     0.0f,
                     CrownHalfWidth_ * kCrossOfCrown * Rng_.Signed()}};
    } else {
      t.Pos = Vec3f{{stool * std::cos(roll), 0.0f, stool * std::sin(roll)}};
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
    Vec3f pos, Vec3f dir, Vec3f up, float radius, int count, float roll) {
  Vec3f n;
  Vec3f b;
  FrameFrom(dir, up, n, b);
  count = std::max(count, 1);
  constexpr float kAzimuthJitterRad = 0.30f;
  constexpr float kSeatOfRadius = 0.95f;
  constexpr float kForwardTilt = 0.40f;
  for (int i = 0; i < count; ++i) {
    const float az = roll + kTau * (static_cast<float>(i) / static_cast<float>(count)) +
                     Rng_.Signed() * kAzimuthJitterRad;
    const Vec3f radial = n * std::cos(az) + b * std::sin(az);
    Plant_->LeafPoints.push_back(LeafPoint{.Pos = pos + radial * (radius * kSeatOfRadius),
                                           .Dir = DirectionOrUp(radial + dir * kForwardTilt)});
  }
}

void TreeGrower::SpawnLateral(
    const Tip &t, const TreeSpecies::Growth &g, int node, float roll, int parentStep) {
  const Vec3f fn = RadialAt(t.Dir, t.Up, roll);
  const float a = (g.BranchAngle + Rng_.Signed() * g.BranchAngleVar) * kDeg;
  const Vec3f dir = DirectionOrUp(t.Dir * std::cos(a) + fn * std::sin(a));
  const float br = t.Radius * g.OrderRadius;
  if (br <= g.MinRadius) { return; }

  if (g.ShadePrune > 0.0f && t.Order >= 2) {
    const float rl = std::sqrt(t.Pos[0] * t.Pos[0] + t.Pos[2] * t.Pos[2]);
    if (rl > kLeastRunM) {
      const float dOut = (dir[0] * t.Pos[0] + dir[2] * t.Pos[2]) / rl;
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
  const Vec3f from = (upper.Pos + lower.Pos) * 0.5f + RadialAt(upper.Dir, upper.Up, request.Roll) *
                                                          (0.5f * (upper.Radius + lower.Radius));

  int steps = static_cast<int>(static_cast<float>(parent.Steps) * g.OrderLen);
  steps = std::max(steps, kMinBranchSteps);
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

  for (auto t : Queue_) {
    Plant_->Shoots[static_cast<size_t>(t.Shoot)].First = static_cast<int>(Plant_->Nodes.size());
    float leafRoll = t.Roll;

    {
      Vec3f n;
      Vec3f b;
      FrameFrom(t.Dir, t.Up, n, b);
      t.Up = n;
    }
    AddNode(t.Shoot, t.Pos, t.Dir, t.Up, t.Radius);
    if (Plant_->Shoots[static_cast<size_t>(t.Shoot)].Parent >= 0) {
      t.Pos = t.Pos + t.Dir * t.Step;
      AddNode(t.Shoot, t.Pos, t.Dir, t.Up, t.Radius);
    } else if (t.Leader == 0) {
      TrunkProfile_.push_back(Vec3f{{t.Pos[1], t.Radius, 0.0f}});
    }

    int last = static_cast<int>(Plant_->Nodes.size()) - 1;
    for (int s = 0; s < t.Steps; ++s) {
      if (static_cast<int>(Plant_->Nodes.size()) >= kMaxNodes) { break; }
      const Vec3f oldDir = t.Dir;

      Vec3f nf;
      Vec3f bf;
      FrameFrom(t.Dir, t.Up, nf, bf);
      const float wr = g.Wander * kDeg;
      const float ub = (t.Order == 0) ? g.LeaderBias : g.BranchUpBias;
      Vec3f want = t.Dir;
      want = want + nf * (Rng_.Signed() * wr);
      want = want + bf * (Rng_.Signed() * wr);
      want = want + Vec3f{{0, 1, 0}} * ub;
      t.Dir = DirectionOrUp(want);

      const Vec3f nPos = t.Pos + t.Dir * t.Step;
      Vec3f up = RmfDouble(t.Pos, nPos, oldDir, t.Dir, t.Up);
      up = DirectionOrUp(up - t.Dir * Dot(up, t.Dir));
      t.Up = up;
      t.Pos = nPos;
      t.Radius = t.Radius * g.Taper;
      last = AddNode(t.Shoot, t.Pos, t.Dir, t.Up, t.Radius);
      if (t.Order == 0 && t.Leader == 0) {
        TrunkProfile_.push_back(Vec3f{{t.Pos[1], t.Radius, 0.0f}});
      }

      const float escaped = Escape(t.Pos);
      if (escaped > 1.0f) {
        constexpr float kEscapeToFull = 0.25f;
        const float pull = std::fmin(1.0f, (escaped - 1.0f) / kEscapeToFull) * kBendBack;
        t.Dir = DirectionOrUp(t.Dir + Inward(t.Pos) * pull);
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
        const Vec3f fn = RadialAt(t.Dir, t.Up, roll);
        const Vec3f dir = DirectionOrUp(t.Dir + fn * 0.55f);
        SpawnShoot(t,
                   Request{.ParentNode = last,
                           .Roll = roll,
                           .Dir = dir,
                           .Up = fn,
                           .Radius = t.Radius * kTipTaper,
                           .Foliate = t.Foliate},
                   g);
      }
    }
    if (t.Foliate &&
        ((t.Order >= 1) || (g.FoliageOnLeader && t.Radius < leafThreshold * kLeafRadiusFactor)) &&
        t.Radius < leafThreshold * kLeafRadiusFactor) {
      EmitLeafPoints(t.Pos, t.Dir, t.Up, t.Radius, kLeafPointsPerWhorl, leafRoll + 1.1f);
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
    const Vec3f anchor = nodes[static_cast<size_t>(s.First)].Pos;
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
  Vec3f mn = Vec3f{{static_cast<float>(kBeyondAnyCoordinate),
                    static_cast<float>(kBeyondAnyCoordinate),
                    static_cast<float>(kBeyondAnyCoordinate)}};
  Vec3f mx = Vec3f{{-static_cast<float>(kBeyondAnyCoordinate),
                    -static_cast<float>(kBeyondAnyCoordinate),
                    -static_cast<float>(kBeyondAnyCoordinate)}};
  const auto cover = [&mn, &mx](Vec3f p, Vec3f half) {
    mn = Vec3f{{std::fmin(mn[0], p[0] - half[0]),
                std::fmin(mn[1], p[1] - half[1]),
                std::fmin(mn[2], p[2] - half[2])}};
    mx = Vec3f{{std::fmax(mx[0], p[0] + half[0]),
                std::fmax(mx[1], p[1] + half[1]),
                std::fmax(mx[2], p[2] + half[2])}};
  };

  for (const TreeSkeleton::Shoot &s : Plant_->Shoots) {
    for (int i = s.First; i < s.First + s.Count; ++i) {
      const TreeSkeleton::Node &n = Plant_->Nodes[static_cast<size_t>(i)];
      const Vec3f disc = Vec3f{{n.Radius * std::sqrt(std::fmax(0.0f, 1.0f - n.Dir[0] * n.Dir[0])),
                                n.Radius * std::sqrt(std::fmax(0.0f, 1.0f - n.Dir[1] * n.Dir[1])),
                                n.Radius * std::sqrt(std::fmax(0.0f, 1.0f - n.Dir[2] * n.Dir[2]))}};
      cover(n.Pos, disc);
      if (i == s.First && s.Parent < 0) { cover(n.Pos - n.Dir * (0.6f * n.Radius), disc); }
      if (i + 1 == s.First + s.Count) {
        const float apex =
            s.End == RingCap::Point ? 2.4f : (s.End == RingCap::Broken ? 1.4f : 0.0f);
        cover(n.Pos + n.Dir * (apex * n.Radius), disc);
      }
    }
  }
  for (const LeafPoint &p : Plant_->LeafPoints) { cover(p.Pos, Vec3f{}); }

  const bool lying = GrowthForm::Lying(Form_.Arch);
  const float y0 =
      lying ? mn[1]
            : (TrunkProfile_.empty() ? mn[1] : TrunkProfile_[0][0] - 0.6f * TrunkProfile_[0][1]);
  float h = lying ? std::fmax(mx[0] - mn[0], mx[2] - mn[2]) : mx[1] - y0;
  if (h < 1e-6f) { h = 1.0f; }
  GrowHeight_ = h;
  const float s = 1.0f / h;
  for (TreeSkeleton::Node &n : Plant_->Nodes) {
    n.Pos = Vec3f{{n.Pos[0] * s, (n.Pos[1] - y0) * s, n.Pos[2] * s}};
    n.Radius *= s;
  }
  for (LeafPoint &p : Plant_->LeafPoints) {
    p.Pos = Vec3f{{p.Pos[0] * s, (p.Pos[1] - y0) * s, p.Pos[2] * s}};
  }
  for (TreeSkeleton::Shoot &shoot : Plant_->Shoots) { shoot.Reach *= s; }
  Plant_->BoxMin = Vec3f{{mn[0] * s, (mn[1] - y0) * s, mn[2] * s}};
  Plant_->BoxMax = Vec3f{{mx[0] * s, (mx[1] - y0) * s, mx[2] * s}};

  if (TrunkProfile_.empty()) { return; }
  Plant_->FootRadius = TrunkProfile_[0][1] * s;
  Plant_->DbhRadius = Plant_->FootRadius;
  if (heightM <= 0.0f) { return; }
  const float yb = 1.3f / heightM;
  for (size_t i = 1; i < TrunkProfile_.size(); ++i) {
    const float ya = (TrunkProfile_[i - 1][0] - y0) * s;
    const float yc = (TrunkProfile_[i][0] - y0) * s;
    if (yb > yc) { continue; }
    float u = yc > ya ? (yb - ya) / (yc - ya) : 0.0f;
    u = std::max(u, 0.0f);
    Plant_->DbhRadius =
        (TrunkProfile_[i - 1][1] + (TrunkProfile_[i][1] - TrunkProfile_[i - 1][1]) * u) * s;
    return;
  }
  Plant_->DbhRadius = TrunkProfile_.back()[1] * s;
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

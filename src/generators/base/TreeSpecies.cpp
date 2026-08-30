#include "TreeSpecies.h"

#include <cstring>
#include <optional>

#include "Json.h"

namespace outshine::Generators {

namespace {

float NumF(const Json::Ref &r, const char *key, float def) {
  return (float)r[key].Num((double)def);
}

int NumI(const Json::Ref &r, const char *key, int def) {
  return r[key].Int(def);
}

[[nodiscard]] bool NumB(const Json::Ref &r, const char *key, bool def) {
  return r[key].Int(def ? 1 : 0) != 0;
}

[[nodiscard]] TreeSpecies::LeafKind KindOf(const std::string &s) {
  if (s == "needle") { return TreeSpecies::LeafKind::Needle; }
  if (s == "palmate") { return TreeSpecies::LeafKind::Palmate; }
  if (s == "pinnate") { return TreeSpecies::LeafKind::Pinnate; }
  if (s == "palmate_compound") { return TreeSpecies::LeafKind::PalmateCompound; }
  return TreeSpecies::LeafKind::Broad;
}

}

bool TreeSpecies::Parse(const char *text, size_t len) {
  Json doc;
  if (!doc.Parse(text, len)) {
    Error_ = "parse failed";
    return false;
  }
  const Json::Ref r = doc.Root();
  if (r.GetKind() != Json::Kind::Object) {
    Error_ = "root is not an object";
    return false;
  }
  Name_ = r["name"].Str();
  Botanical_ = r["botanical"].Str();
  if (Name_.empty()) {
    Error_ = "no name";
    return false;
  }
  HeightM_ = NumF(r, "height_m", HeightM_);
  SpreadM_ = NumF(r, "spread_m", SpreadM_);
  HeightSigma_ = NumF(r, "height_sigma", HeightSigma_);
  DbhM_ = NumF(r, "dbh_cm", 0.0f) * 0.01f;
  Lai_ = NumF(r, "lai", 0.0f);

  GrowthForm &f = Form_;
  const std::string arch = r["form"].Str("single_stem_tree");
  const std::string crown = r["crown"].Str("free");
  const std::optional<Architecture> archOf = GrowthForm::ArchitectureOf(arch.c_str());
  const std::optional<CrownEnvelope> crownOf = GrowthForm::EnvelopeOf(crown.c_str());
  if (!archOf) {
    Error_ = "form: unknown value '" + arch + "'";
    return false;
  }
  if (!crownOf) {
    Error_ = "crown: unknown value '" + crown + "'";
    return false;
  }
  f.Arch = *archOf;
  f.Envelope = *crownOf;
  f.Leaders = NumI(r, "leaders", f.Leaders);
  f.LeaderSplayDeg = NumF(r, "leader_splay", f.LeaderSplayDeg);
  f.BoleFrac = NumF(r, "bole_frac", f.BoleFrac);
  f.BreakFrac = NumF(r, "break_frac", f.BreakFrac);
  f.RunM = NumF(r, "run_m", f.RunM);
  f.Foliate = NumB(r, "foliate", f.Foliate);

  Growth &g = Growth_;
  g.Seed = (uint32_t)NumI(r, "seed", (int)g.Seed);
  g.TrunkSides = NumI(r, "trunk_sides", g.TrunkSides);
  g.BaseRadius = NumF(r, "base_radius", g.BaseRadius);
  g.StepLen = NumF(r, "step_len", g.StepLen);
  g.TrunkSteps = NumI(r, "trunk_steps", g.TrunkSteps);
  g.Taper = NumF(r, "taper", g.Taper);
  g.MinRadius = NumF(r, "min_radius", g.MinRadius);
  g.TwigRadius = NumF(r, "twig_radius", g.TwigRadius);
  g.BranchChance = NumF(r, "branch_chance", g.BranchChance);
  g.MaxOrder = NumI(r, "max_order", g.MaxOrder);
  g.TerminalFork = NumB(r, "terminal_fork", g.TerminalFork);
  g.BranchAngle = NumF(r, "branch_angle", g.BranchAngle);
  g.BranchAngleVar = NumF(r, "branch_angle_var", g.BranchAngleVar);
  g.OrderLen = NumF(r, "order_len", g.OrderLen);
  g.OrderRadius = NumF(r, "order_radius", g.OrderRadius);
  g.Wander = NumF(r, "wander", g.Wander);
  g.LeaderBias = NumF(r, "leader_bias", g.LeaderBias);
  g.BranchUpBias = NumF(r, "branch_up_bias", g.BranchUpBias);
  g.WhorlCount = NumI(r, "whorl_count", g.WhorlCount);
  g.WhorlSpacing = NumI(r, "whorl_spacing", g.WhorlSpacing);
  g.FoliageFactor = NumF(r, "foliage_factor", g.FoliageFactor);
  g.FoliageOnLeader = NumB(r, "foliage_on_leader", g.FoliageOnLeader);
  g.ShadePrune = NumF(r, "shade_prune", g.ShadePrune);

  Leaf &l = Leaf_;
  l.Kind = KindOf(r["leaf_kind"].Str("broad"));
  l.Segments = NumI(r, "leaf_segments", l.Segments);
  l.Length = NumF(r, "leaf_length", l.Length);
  l.Width = NumF(r, "leaf_width", l.Width);
  l.Widest = NumF(r, "leaf_widest", l.Widest);
  l.BaseFill = NumF(r, "leaf_base_fill", l.BaseFill);
  l.BaseSkew = NumF(r, "leaf_base_skew", l.BaseSkew);
  l.Tip = NumF(r, "leaf_tip", l.Tip);
  l.Lobes = NumI(r, "leaf_lobes", l.Lobes);
  l.LobeDepth = NumF(r, "leaf_lobe_depth", l.LobeDepth);
  l.Serration = NumF(r, "leaf_serration", l.Serration);
  l.Fold = NumF(r, "leaf_fold", l.Fold);
  l.Curve = NumF(r, "leaf_curve", l.Curve);
  l.Leaflets = NumI(r, "leaf_leaflets", l.Leaflets);
  l.PalmateLobes = NumI(r, "leaf_palmate_lobes", l.PalmateLobes);
  l.PalmateSpread = NumF(r, "leaf_palmate_spread", l.PalmateSpread);
  l.NeedleWidth = NumF(r, "leaf_needle_width", l.NeedleWidth);
  l.NeedleLen = NumF(r, "leaf_needle_len", l.NeedleLen);
  l.NeedleFwd = NumF(r, "leaf_needle_fwd", l.NeedleFwd);
  l.Droop = NumB(r, "leaf_droop", l.Droop);
  l.CardW = NumF(r, "leaf_card_w", l.CardW);
  l.CardH = NumF(r, "leaf_card_h", l.CardH);
  l.CardsPerPoint = NumI(r, "leaf_cards", l.CardsPerPoint);
  l.CardBudget = NumI(r, "leaf_card_budget", l.CardBudget);

  Shading &s = Shading_;
  s.BarkColor[0] = NumF(r, "bark_r", s.BarkColor[0]);
  s.BarkColor[1] = NumF(r, "bark_g", s.BarkColor[1]);
  s.BarkColor[2] = NumF(r, "bark_b", s.BarkColor[2]);
  s.BarkDark = NumF(r, "bark_dark", s.BarkDark);
  s.BarkFreq = NumF(r, "bark_freq", s.BarkFreq);
  s.BarkRidge = NumF(r, "bark_ridge", s.BarkRidge);
  s.BarkStyle = NumI(r, "bark_style", s.BarkStyle);
  s.LeafTint[0] = NumF(r, "leaf_r", s.LeafTint[0]);
  s.LeafTint[1] = NumF(r, "leaf_g", s.LeafTint[1]);
  s.LeafTint[2] = NumF(r, "leaf_b", s.LeafTint[2]);
  s.WindAmp = NumF(r, "wind_amp", s.WindAmp);
  s.WindFreq = NumF(r, "wind_freq", s.WindFreq);

  Error_.clear();
  if (Growth_.WhorlCount > 0 && Growth_.WhorlSpacing <= 0) {
    Error_ = Name_ + " declares whorl_count " + std::to_string(Growth_.WhorlCount) +
             " with whorl_spacing " + std::to_string(Growth_.WhorlSpacing) +
             " -- a whorl every zero steps divides by it";
    return false;
  }
  if (HeightM_ <= 0.0f) {
    Error_ = Name_ + " declares height_m <= 0, and a tree without height is not a tree";
    return false;
  }
  if (Growth_.MaxOrder < 0 || Growth_.MaxOrder > 8) {
    Error_ = Name_ + " declares max_order outside 0..8, the grower's own bound";
    return false;
  }
  return true;
}
}

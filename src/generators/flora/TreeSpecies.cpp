#include "TreeSpecies.h"
#include "TreeLeaf.h"

#include <cstdint>
#include <array>
#include <span>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "Json.h"

namespace outshine::Generators {

constexpr float kCmToM = 0.01f;

namespace {

float NumF(const Json::Ref &r, const char *key, float def) {
  return static_cast<float>(r[key].Num(static_cast<double>(def)));
}

int NumI(const Json::Ref &r, const char *key, int def) {
  return r[key].Int(def);
}

[[nodiscard]] bool NumB(const Json::Ref &r, const char *key, bool def) {
  return r[key].Int(def ? 1 : 0) != 0;
}

[[nodiscard]] std::optional<TreeSpecies::LeafKind> KindOf(const std::string &s) {
  if (s == "needle") { return TreeSpecies::LeafKind::Needle; }
  if (s == "palmate") { return TreeSpecies::LeafKind::Palmate; }
  if (s == "pinnate") { return TreeSpecies::LeafKind::Pinnate; }
  if (s == "palmate_compound") { return TreeSpecies::LeafKind::PalmateCompound; }
  if (s == "broad") { return TreeSpecies::LeafKind::Broad; }
  return std::nullopt;
}

struct NumberRange {
  double Least;
  double Most;
  bool Integral = false;
};

std::optional<std::string_view>
InvalidNumber(const Json::Ref &root, std::span<const char *const> names, NumberRange range) {
  for (const char *key : names) {
    const auto value = root[key];
    if (!value.Valid()) { continue; }
    const double number = value.Num();
    if (value.GetKind() != Json::Kind::Number || !std::isfinite(number) || number < range.Least ||
        number > range.Most || (range.Integral && std::trunc(number) != number)) {
      return key;
    }
  }
  return std::nullopt;
}

std::optional<std::string_view> InvalidSpeciesNumber(const Json::Ref &root) {
  constexpr std::array integers{"bark_style",
                                "leaders",
                                "leaf_card_budget",
                                "leaf_cards",
                                "leaf_leaflets",
                                "leaf_lobes",
                                "leaf_palmate_lobes",
                                "leaf_segments",
                                "max_order",
                                "trunk_sides",
                                "trunk_steps",
                                "whorl_count",
                                "whorl_spacing"};
  constexpr std::array reals{"bark_b",
                             "bark_dark",
                             "bark_freq",
                             "bark_g",
                             "bark_r",
                             "bark_ridge",
                             "bark_roughness",
                             "base_radius",
                             "bole_frac",
                             "branch_angle",
                             "branch_angle_var",
                             "branch_chance",
                             "branch_up_bias",
                             "break_frac",
                             "dbh_cm",
                             "foliage_factor",
                             "height_m",
                             "height_sigma",
                             "lai",
                             "leader_bias",
                             "leader_splay",
                             "leaf_b",
                             "leaf_base_fill",
                             "leaf_base_skew",
                             "leaf_card_h",
                             "leaf_card_w",
                             "leaf_curve",
                             "leaf_fold",
                             "leaf_g",
                             "leaf_length",
                             "leaf_lobe_depth",
                             "leaf_needle_fwd",
                             "leaf_needle_len",
                             "leaf_needle_width",
                             "leaf_palmate_spread",
                             "leaf_r",
                             "leaf_roughness",
                             "leaf_serration",
                             "leaf_tip",
                             "leaf_widest",
                             "leaf_width",
                             "min_radius",
                             "order_len",
                             "order_radius",
                             "run_m",
                             "shade_prune",
                             "spread_m",
                             "step_len",
                             "taper",
                             "twig_radius",
                             "wander",
                             "wind_amp",
                             "wind_freq"};
  if (auto key = InvalidNumber(root,
                               integers,
                               {.Least = std::numeric_limits<int>::min(),
                                .Most = std::numeric_limits<int>::max(),
                                .Integral = true})) {
    return key;
  }
  if (auto key = InvalidNumber(root,
                               reals,
                               {.Least = -std::numeric_limits<float>::max(),
                                .Most = std::numeric_limits<float>::max()})) {
    return key;
  }
  if (auto key = InvalidNumber(
          root,
          std::array{"seed"},
          {.Least = 0, .Most = std::numeric_limits<uint32_t>::max(), .Integral = true})) {
    return key;
  }

  struct Bounded {
    const char *Key;
    NumberRange Range;
  };

  constexpr double kMostBranches = 64;
  constexpr double kMostSteps = 4096;
  constexpr std::array bounded{
      Bounded{.Key = "leaders", .Range = {.Least = 1, .Most = kMostBranches}},
      Bounded{.Key = "trunk_sides", .Range = {.Least = 3, .Most = kMostBranches}},
      Bounded{.Key = "trunk_steps", .Range = {.Least = 1, .Most = kMostSteps}},
      Bounded{.Key = "whorl_count", .Range = {.Least = 0, .Most = kMostBranches}},
      Bounded{.Key = "whorl_spacing", .Range = {.Least = 1, .Most = kMostSteps}},
      Bounded{.Key = "max_order", .Range = {.Least = 0, .Most = 8}},
      Bounded{.Key = "bole_frac", .Range = {.Least = 0, .Most = 1}},
      Bounded{.Key = "break_frac", .Range = {.Least = 0, .Most = 1}},
      Bounded{.Key = "order_len", .Range = {.Least = 0, .Most = 1}}};
  for (const auto &one : bounded) {
    if (auto key = InvalidNumber(root, std::array{one.Key}, one.Range)) { return key; }
  }
  return std::nullopt;
}

}

bool TreeSpecies::Parse(const char *text, size_t len) {
  TreeSpecies parsed;
  if (!parsed.Read(text, len)) {
    Error_ = std::move(parsed.Error_);
    return false;
  }
  parsed.Definition_.assign(text, len);
  *this = std::move(parsed);
  return true;
}

bool TreeSpecies::Read(const char *text, size_t len) {
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
  if (const auto key = InvalidSpeciesNumber(r)) {
    Error_ = "invalid numeric species field: " + std::string(*key);
    return false;
  }
  HeightM_ = NumF(r, "height_m", HeightM_);
  SpreadM_ = NumF(r, "spread_m", SpreadM_);
  HeightSigma_ = NumF(r, "height_sigma", HeightSigma_);
  DbhM_ = NumF(r, "dbh_cm", 0.0f) * kCmToM;
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
  g.Seed = static_cast<uint32_t>(r["seed"].Num(static_cast<double>(g.Seed)));
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

  const auto kind = r["leaf_kind"];
  const auto leafKind = KindOf(kind.Valid() ? kind.Str() : "broad");
  if (!leafKind) {
    Error_ = "unknown leaf_kind";
    return false;
  }
  Leaf &l = Leaf_;
  l.Kind = *leafKind;
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

  if (const auto valid = TreeLeaf::Validate(l); !valid) {
    Error_ = valid.error();
    return false;
  }

  Shading &s = Shading_;
  s.BarkRoughness = NumF(r, "bark_roughness", s.BarkRoughness);
  s.LeafRoughness = NumF(r, "leaf_roughness", s.LeafRoughness);
  if (!std::ranges::all_of(std::array{s.BarkRoughness, s.LeafRoughness},
                           [](float value) { return value >= 0 && value <= 1; })) {
    Error_ = "bark_roughness and leaf_roughness must be in [0, 1]";
    return false;
  }
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

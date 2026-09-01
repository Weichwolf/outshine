#ifndef OUTSHINE_GENERATORS_BASE_TREESPECIES_H
#define OUTSHINE_GENERATORS_BASE_TREESPECIES_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "math/Vec3.h"
#include "GrowthForm.h"

namespace outshine::Generators {

class TreeSpecies {
public:
  enum class LeafKind : uint8_t { Broad, Needle, Palmate, Pinnate, PalmateCompound };

  struct Growth {
    uint32_t Seed;
    int TrunkSides;
    float BaseRadius;
    float StepLen;
    int TrunkSteps;
    float Taper;
    float MinRadius;
    float TwigRadius;
    float BranchChance;
    int MaxOrder;
    bool TerminalFork;
    float BranchAngle;
    float BranchAngleVar;
    float OrderLen;
    float OrderRadius;
    float Wander;
    float LeaderBias;
    float BranchUpBias;
    int WhorlCount;
    int WhorlSpacing;
    float FoliageFactor;
    bool FoliageOnLeader;
    float ShadePrune;
  };

  static constexpr Growth kGrowthUnsaid = {
      .Seed = 1,
      .TrunkSides = 10,
      .BaseRadius = 0.07f,
      .StepLen = 0.16f,
      .TrunkSteps = 26,
      .Taper = 0.955f,
      .MinRadius = 0.005f,
      .TwigRadius = 0.013f,
      .BranchChance = 0.85f,
      .MaxOrder = 3,
      .TerminalFork = true,
      .BranchAngle = 55.0f,
      .BranchAngleVar = 12.0f,
      .OrderLen = 0.62f,
      .OrderRadius = 0.52f,
      .Wander = 7.0f,
      .LeaderBias = 0.18f,
      .BranchUpBias = 0.12f,
      .WhorlCount = 0,
      .WhorlSpacing = 4,
      .FoliageFactor = 5.5f,
      .FoliageOnLeader = false,
      .ShadePrune = 0.0f,
  };

  struct Leaf {
    LeafKind Kind;
    int Segments;
    float Length;
    float Width;
    float Widest;
    float BaseFill;
    float BaseSkew;
    float Tip;
    int Lobes;
    float LobeDepth;
    float Serration;
    float Fold;
    float Curve;
    int Leaflets;
    int PalmateLobes;
    float PalmateSpread;
    float NeedleWidth;
    float NeedleLen;
    float NeedleFwd;
    bool Droop;

    float CardW, CardH;
    int CardsPerPoint, CardBudget;
  };

  static constexpr Leaf kLeafUnsaid = {
      .Kind = LeafKind::Broad,
      .Segments = 28,
      .Length = 1.0f,
      .Width = 0.34f,
      .Widest = 0.45f,
      .BaseFill = 0.0f,
      .BaseSkew = 0.0f,
      .Tip = 0.5f,
      .Lobes = 0,
      .LobeDepth = 0.0f,
      .Serration = 0.0f,
      .Fold = 0.10f,
      .Curve = 0.16f,
      .Leaflets = 0,
      .PalmateLobes = 5,
      .PalmateSpread = 78.0f,
      .NeedleWidth = 0.03f,
      .NeedleLen = 0.17f,
      .NeedleFwd = 0.64f,
      .Droop = false,
      .CardW = 0.075f,
      .CardH = 0.10f,
      .CardsPerPoint = 3,
      .CardBudget = 2600,
  };

  struct Shading {
    Vec3f BarkColor;
    float BarkDark;
    float BarkFreq;
    float BarkRidge;
    int BarkStyle;
    Vec3f LeafTint;
    float WindAmp, WindFreq;
  };

  static constexpr Shading kShadingUnsaid = {
      .BarkColor = {{0.40f, 0.31f, 0.23f}},
      .BarkDark = 0.62f,
      .BarkFreq = 4.0f,
      .BarkRidge = 0.2f,
      .BarkStyle = 0,
      .LeafTint = {{1.0f, 1.0f, 1.0f}},
      .WindAmp = 0.012f,
      .WindFreq = 1.6f,
  };

  [[nodiscard]] bool Parse(const char *text, size_t len);

  [[nodiscard]] const std::string &Error() const { return Error_; }

  [[nodiscard]] const std::string &Name() const { return Name_; }

  [[nodiscard]] const std::string &Botanical() const { return Botanical_; }

  [[nodiscard]] const GrowthForm &Form() const { return Form_; }

  [[nodiscard]] const Growth &GrowthParams() const { return Growth_; }

  [[nodiscard]] const Leaf &LeafParams() const { return Leaf_; }

  [[nodiscard]] const Shading &ShadingParams() const { return Shading_; }

  [[nodiscard]] float HeightM() const { return HeightM_; }

  [[nodiscard]] float SpreadM() const { return SpreadM_; }

  [[nodiscard]] float HeightSigma() const { return HeightSigma_; }

  [[nodiscard]] float DbhM() const { return DbhM_; }

  [[nodiscard]] float Lai() const { return Lai_; }

private:
  std::string Error_, Name_, Botanical_;
  GrowthForm Form_;
  Growth Growth_ = kGrowthUnsaid;
  Leaf Leaf_ = kLeafUnsaid;
  Shading Shading_ = kShadingUnsaid;
  static constexpr float kHeightUnsaidM = 20.0f;
  static constexpr float kSpreadUnsaidM = 10.0f;

  float HeightM_ = kHeightUnsaidM;
  float SpreadM_ = kSpreadUnsaidM;

  float HeightSigma_ = 0.0f;
  float DbhM_ = 0.0f;
  float Lai_ = 0.0f;
};

} // namespace outshine::Generators
#endif

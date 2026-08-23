#ifndef OUTSHINE_GENERATORS_TREESPECIES_H
#define OUTSHINE_GENERATORS_TREESPECIES_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "GrowthForm.h"

namespace outshine::Generators {

class TreeSpecies {
public:
  enum class LeafKind : uint8_t { Broad, Needle, Palmate, Pinnate, PalmateCompound };

  struct Growth {
    uint32_t Seed = 1;
    int TrunkSides = 10;
    float BaseRadius = 0.07f;
    float StepLen = 0.16f;
    int TrunkSteps = 26;
    float Taper = 0.955f;
    float MinRadius = 0.005f;
    float TwigRadius = 0.013f;
    float BranchChance = 0.85f;
    int MaxOrder = 3;
    bool TerminalFork = true;
    float BranchAngle = 55.0f;
    float BranchAngleVar = 12.0f;
    float OrderLen = 0.62f;
    float OrderRadius = 0.52f;
    float Wander = 7.0f;
    float LeaderBias = 0.18f;
    float BranchUpBias = 0.12f;
    int WhorlCount = 0;
    int WhorlSpacing = 4;
    float FoliageFactor = 5.5f;
    bool FoliageOnLeader = false;
    float ShadePrune = 0.0f;
  };

  struct Leaf {
    LeafKind Kind = LeafKind::Broad;
    int Segments = 28;
    float Length = 1.0f;
    float Width = 0.34f;
    float Widest = 0.45f;
    float BaseFill = 0.0f;
    float BaseSkew = 0.0f;
    float Tip = 0.5f;
    int Lobes = 0;
    float LobeDepth = 0.0f;
    float Serration = 0.0f;
    float Fold = 0.10f;
    float Curve = 0.16f;
    int Leaflets = 0;
    int PalmateLobes = 5;
    float PalmateSpread = 78.0f;
    float NeedleWidth = 0.03f;
    float NeedleLen = 0.17f;
    float NeedleFwd = 0.64f;
    bool Droop = false;

    float CardW = 0.075f, CardH = 0.10f;
    int CardsPerPoint = 3, CardBudget = 2600;
  };

  struct Shading {
    float BarkColor[3] = {0.40f, 0.31f, 0.23f};
    float BarkDark = 0.62f;
    float BarkFreq = 4.0f;
    float BarkRidge = 0.2f;
    int BarkStyle = 0;
    float LeafTint[3] = {1.0f, 1.0f, 1.0f};
    float WindAmp = 0.012f, WindFreq = 1.6f;
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
  Growth Growth_;
  Leaf Leaf_;
  Shading Shading_;
  float HeightM_ = 20.0f;
  float SpreadM_ = 10.0f;

  float HeightSigma_ = 0.0f;
  float DbhM_ = 0.0f;
  float Lai_ = 0.0f;
};

}
#endif

/* ONE SPECIES = ONE JSON FILE under sim/assets/world/species/, and nothing else declares a tree.
 * Three groups, because three different things read them: Growth drives TreeGrower, Leaf drives
 * TreeLeaf, Shading drives nothing yet and is the bark/foliage/wind the draw call will want. */
#ifndef TREESPECIES_H
#define TREESPECIES_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace outshine::World {

class TreeSpecies {
public:
  enum class LeafKind : uint8_t { Broad, Needle, Palmate, Pinnate, PalmateCompound };

  struct Growth {
    uint32_t Seed = 1;
    int TrunkSides = 10;
    float BaseRadius = 0.07f;
    float StepLen = 0.16f;
    int TrunkSteps = 26;
    int BareSteps = 9;      /* lower steps that carry no branch — a clear bole under the crown */
    float Taper = 0.955f;
    float MinRadius = 0.005f;
    float TwigRadius = 0.013f;
    float BranchChance = 0.85f;
    int MaxOrder = 3;
    bool TerminalFork = true;
    float BranchAngle = 55.0f;    /* degrees FROM THE PARENT AXIS: 90 horizontal, >90 hanging */
    float BranchAngleVar = 12.0f;
    float OrderLen = 0.62f;
    float OrderRadius = 0.52f;
    float Wander = 7.0f;
    float LeaderBias = 0.18f;
    float BranchUpBias = 0.12f;
    float Conical = 0.0f;
    int WhorlCount = 0;     /* 0 = spiral phyllotaxis; >0 = N branches per whorl (conifers) */
    int WhorlSpacing = 4;
    float FoliageFactor = 5.5f;
    bool FoliageOnLeader = false;
    float CrownBase = 0.0f;
    float ShadePrune = 0.0f;
  };

  struct Leaf {
    LeafKind Kind = LeafKind::Broad;
    int Segments = 28;
    float Length = 1.0f;
    float Width = 0.34f;    /* max half width as a fraction of the length */
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
    /* Declared by eight of the sixteen and read by nothing in this tree: they belong to the leaf-card
     * stage, which does not exist. Parsed so that a species file stays whole. */
    float CardW = 0.075f, CardH = 0.10f;
    int CardsPerPoint = 3, CardBudget = 2600;
  };

  struct Shading {
    float BarkColor[3] = {0.40f, 0.31f, 0.23f};
    float BarkDark = 0.62f;   /* furrow colour = BarkColor * BarkDark */
    float BarkFreq = 4.0f;
    float BarkRidge = 0.2f;
    int BarkStyle = 0;
    float LeafTint[3] = {1.0f, 1.0f, 1.0f};
    float WindAmp = 0.012f, WindFreq = 1.6f;
  };

  bool Parse(const char *text, size_t len);

  const std::string &Error() const { return Error_; }
  const std::string &Name() const { return Name_; }
  const std::string &Botanical() const { return Botanical_; }
  const Growth &GrowthParams() const { return Growth_; }
  const Leaf &LeafParams() const { return Leaf_; }
  const Shading &ShadingParams() const { return Shading_; }
  float HeightM() const { return HeightM_; }
  float SpreadM() const { return SpreadM_; }
  float HeightSigma() const { return HeightSigma_; }
  /* Breast-height diameter at 1.3 m, in METRES. The only metric a stem carries, and the grower solves
   * its radius cascade against it. 0 leaves the declared `base_radius` alone. */
  float BhdM() const { return BhdM_; }

private:
  std::string Error_, Name_, Botanical_;
  Growth Growth_;
  Leaf Leaf_;
  Shading Shading_;
  float HeightM_ = 20.0f;
  float SpreadM_ = 10.0f;
  /* 0 heisst "eine Art ohne gemessene Bestandesstreuung", nicht "kein Baum streut": wer eine Art in
   * ein Feld stellt, ohne sie zu deklarieren, bekommt sichtbar identische Hoehen. */
  float HeightSigma_ = 0.0f;
  float BhdM_ = 0.0f;
};

} // namespace outshine::World
#endif

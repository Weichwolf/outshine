#ifndef OUTSHINE_GENERATORS_FLORA_TREEGROWER_H
#define OUTSHINE_GENERATORS_FLORA_TREEGROWER_H

#include <vector>

#include "TreeFrame.h"
#include "TreeRandom.h"
#include "TreeSkeleton.h"
#include "TreeSpecies.h"
#include "math/Vec3.h"

namespace outshine::Generators {

class TreeGrower {
public:
  void Grow(const TreeSpecies &species, TreeSkeleton &out);

  [[nodiscard]] int Passes() const { return Passes_; }

  [[nodiscard]] float DbhErrorRel() const { return DbhErrorRel_; }

  [[nodiscard]] float GrowHeight() const { return GrowHeight_; }

private:
  static constexpr int kMostTreeNodes = 250000;
  static constexpr int kBranchSides = 8;

  struct Tip {
    Vec3f Dir, Up, Pos;
    float Radius = 0.0f;

    float Step = 0.0f;
    int Order = 0, Steps = 0, Bare = 0;

    int Leader = 0;
    int Shoot = 0;
    bool Foliate = true;
    float Roll = 0.0f;
  };

  struct Request {
    int ParentNode = -1;
    float Roll = 0.0f;
    Vec3f Dir, Up;
    float Radius = 0.0f;
    bool Foliate = true;
  };

  struct Sprout {
    int Node = 0;
    int ParentStep = 0;
  };

  void SpawnLateral(const Tip &t, const TreeSpecies::Growth &g, Sprout from, float roll);

  void SpawnShoot(const Tip &parent, const Request &request, const TreeSpecies::Growth &g);
  void EmitLeafPoints(Vec3f pos, Framing over, float radius, int count, float roll);

  void SetCrown(const TreeSpecies &species, float growHeight);
  void SeedLeaders(const TreeSpecies::Growth &g, int bareSteps);

  [[nodiscard]] float Escape(Vec3f p) const;
  [[nodiscard]] Vec3f Inward(Vec3f p) const;

  [[nodiscard]] float RoomInside(Vec3f from, Vec3f dir, float want) const;
  [[nodiscard]] RingCap LeaderEnd() const;
  int AddNode(int shoot, Vec3f pos, Vec3f dir, Vec3f up, float radius);
  void GrowOnce(const TreeSpecies::Growth &g, float heightM);

  void MeasureReach();
  void NormalizeToUnitHeight(float heightM);

  GrowthForm Form_;

  float CrownBaseY_ = 0.0f, CrownTopY_ = 0.0f, CrownHalfWidth_ = 0.0f, HalfRunX_ = 0.0f;

  TreeSkeleton *Plant_ = nullptr;
  std::vector<Tip> Queue_;
  std::vector<Vec3f> TrunkProfile_;
  TreeRandom Rng_{1};
  int Passes_ = 0;
  float DbhErrorRel_ = 0.0f;
  float GrowHeight_ = 0.0f;
};

} // namespace outshine::Generators
#endif

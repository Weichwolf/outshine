#ifndef OUTSHINE_GENERATORS_DRAW_TREEGROWER_H
#define OUTSHINE_GENERATORS_DRAW_TREEGROWER_H

#include <vector>

#include "TreeRandom.h"
#include "TreeSkeleton.h"
#include "TreeSpecies.h"
#include "TreeVec3.h"

namespace outshine::Generators {

class TreeGrower {
public:
  void Grow(const TreeSpecies &species, TreeSkeleton &out);

  [[nodiscard]] int Passes() const { return Passes_; }
  [[nodiscard]] float DbhErrorRel() const { return DbhErrorRel_; }

  [[nodiscard]] float GrowHeight() const { return GrowHeight_; }

private:

  static constexpr int kMaxNodes = 250000;
  static constexpr int kBranchSides = 8;

  struct Tip {
    TreeVec3 Dir, Up, Pos;
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
    TreeVec3 Dir, Up;
    float Radius = 0.0f;
    bool Foliate = true;
  };

  void SpawnLateral(const Tip &t, const TreeSpecies::Growth &g, int node, float roll, int parentStep);

  void SpawnShoot(const Tip &parent, const Request &request, const TreeSpecies::Growth &g);
  void EmitLeafPoints(TreeVec3 pos, TreeVec3 dir, TreeVec3 up, float radius, int count, float roll);

  void SetCrown(const TreeSpecies &species, float growHeight);
  void SeedLeaders(const TreeSpecies::Growth &g, int bareSteps);

  [[nodiscard]] float Escape(TreeVec3 p) const;
  [[nodiscard]] TreeVec3 Inward(TreeVec3 p) const;

  [[nodiscard]] float RoomInside(TreeVec3 from, TreeVec3 dir, float want) const;
  [[nodiscard]] RingCap LeaderEnd() const;
  int AddNode(int shoot, TreeVec3 pos, TreeVec3 dir, TreeVec3 up, float radius);
  void GrowOnce(const TreeSpecies::Growth &g, float heightM);

  void MeasureReach();
  void NormalizeToUnitHeight(float heightM);

  GrowthForm Form_;

  float CrownBaseY_ = 0.0f, CrownTopY_ = 0.0f, CrownHalfWidth_ = 0.0f, HalfRunX_ = 0.0f;

  TreeSkeleton *Plant_ = nullptr;
  std::vector<Tip> Queue_;
  std::vector<TreeVec3> TrunkProfile_;
  TreeRandom Rng_{1};
  int Passes_ = 0;
  float DbhErrorRel_ = 0.0f;
  float GrowHeight_ = 0.0f;
};

}
#endif

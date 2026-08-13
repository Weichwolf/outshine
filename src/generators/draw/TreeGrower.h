/* GROWTH, AND NOTHING ELSE. The plant is grown ONCE from its declaration and its seed, into a
 * TreeSkeleton; what a rank draws from that skeleton is TreeMesher's question and is not asked here.
 * Nothing in this class can see a pixel, which is why a refinement cannot change the tree.
 *
 * THE FORM IS AN INPUT (generators/GrowthForm.h). How many axes leave the base, whether there is a
 * bole, and what silhouette the crown may fill are declared; everything else here is a parameter
 * within that form. The envelope is enforced twice — a branch is SHORTENED at spawn to the length
 * that stays inside the silhouette, and a tip that drifts out anyway is bent back and stopped. That
 * is Weber & Penn's pruning, moved from a post-pass onto the growing tip.
 *
 * Buffers are members and are reused: growing a stand allocates once per grower, not once per tree. */
#ifndef TREEGROWER_H
#define TREEGROWER_H

#include <vector>

#include "TreeRandom.h"
#include "TreeSkeleton.h"
#include "TreeSpecies.h"
#include "TreeVec3.h"

namespace outshine::Generators {

class TreeGrower {
public:
  void Grow(const TreeSpecies &species, TreeSkeleton &out);

  /* How many grow passes the last Grow spent solving the declared DBH, and what it missed by. */
  int Passes() const { return Passes_; }
  float DbhErrorRel() const { return DbhErrorRel_; }
  /* The height the last plant had BEFORE normalisation, in grower units. */
  float GrowHeight() const { return GrowHeight_; }

private:
  /* [SET] The largest plant a declaration may grow, in axis stations. Measured over the 31
   * declarations the largest is 74 522 (beech) and this is 3.35 times that: it is the hard floor
   * under a runaway declaration and must never be what shapes a plant that parses. A budget used to
   * stand here instead — the mesh was regrown coarser until it fitted 200 000 vertices — and that is
   * why the beech's "declared" mesh was itself budget-shaped at 346 268 triangles when the plant it
   * declares is 1 217 604. */
  static constexpr int kMaxNodes = 250000;
  static constexpr int kBranchSides = 8;

  struct Tip {
    TreeVec3 Dir, Up, Pos;
    float Radius = 0.0f;
    /* A SHOOT'S SEGMENT LENGTH IS ITS OWN. The silhouette decides how far a shoot may run and
     * `order_len` how many segments it runs in, so a crown 3.5 m wide carries the same branching
     * detail as one 20 m wide instead of four stubs. */
    float Step = 0.0f;
    int Order = 0, Steps = 0, Bare = 0;
    /* Which leader this shoot descends from. Only the first traces the trunk profile, so a
     * many-stemmed form still has one stem the DBH solve and the foot radius are read off. */
    int Leader = 0;
    int Shoot = 0;
    bool Foliate = true;
    float Roll = 0.0f;
  };
  /* A SHOOT AS IT IS ASKED FOR, before the silhouette has had its say: which node of which parent it
   * leaves from and where round it, where it points, and how thick it leaves the parent. */
  struct Request {
    int ParentNode = -1;
    float Roll = 0.0f;
    TreeVec3 Dir, Up;
    float Radius = 0.0f;
    bool Foliate = true;
  };

  void SpawnLateral(const Tip &t, const TreeSpecies::Growth &g, int node, float roll, int parentStep);
  /* THE ONE PLACE A SHOOT IS ADMITTED. Lateral and terminal fork differ in where they aim and how
   * thick they start; how long a shoot may run inside the silhouette, whether that leaves a branch
   * at all and what the queued tip inherits from its parent is one rule. */
  void SpawnShoot(const Tip &parent, const Request &request, const TreeSpecies::Growth &g);
  void EmitLeafPoints(TreeVec3 pos, TreeVec3 dir, TreeVec3 up, float radius, int count, float roll);
  /* The crown the envelope is measured against, in grower units, from the declared spread and the
   * height the previous pass came out at. */
  void SetCrown(const TreeSpecies &species, float growHeight);
  void SeedLeaders(const TreeSpecies::Growth &g, int bareSteps);
  /* How far outside the declared silhouette a point lies. 1 is exactly on it. */
  float Escape(TreeVec3 p) const;
  TreeVec3 Inward(TreeVec3 p) const;
  /* How much of `want`, along `dir` from `from`, stays inside the silhouette. */
  float RoomInside(TreeVec3 from, TreeVec3 dir, float want) const;
  [[nodiscard]] RingCap LeaderEnd() const;
  int AddNode(int shoot, TreeVec3 pos, TreeVec3 dir, TreeVec3 up, float radius);
  void GrowOnce(const TreeSpecies::Growth &g, float heightM);
  /* Each shoot's reach over its whole sub-tree, in one backward pass — the queue is breadth-first, so
   * a child never precedes its parent. */
  void MeasureReach();
  void NormalizeToUnitHeight(float heightM);

  GrowthForm Form_;
  /* Grower units, all four: the crown's foot and top on the growth axis, the half width the
   * envelope's profile is scaled by, and a hedge section's half run. */
  float CrownBaseY_ = 0.0f, CrownTopY_ = 0.0f, CrownHalfWidth_ = 0.0f, HalfRunX_ = 0.0f;
  /* Non-owning and alive only for the duration of one Grow: the growth walk is spread over a
   * dozen small functions and threading the output through every one of them would cost more
   * arguments than it buys (`I.23`). */
  TreeSkeleton *Plant_ = nullptr;
  std::vector<Tip> Queue_;
  std::vector<TreeVec3> TrunkProfile_; /* X = height above the seed ring, Y = radius, both grower units */
  TreeRandom Rng_{1};
  int Passes_ = 0;
  float DbhErrorRel_ = 0.0f;
  float GrowHeight_ = 0.0f;
};

} // namespace outshine::Generators
#endif

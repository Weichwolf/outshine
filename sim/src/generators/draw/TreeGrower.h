/* GROWTH BY EXTRUSION on ONE closed mesh. A seed ring plus a ground cap; every step pulls the open
 * tip ring forward; a branch is a SIDE FACE of the parent extruded away, so trunk and branches share a
 * topology instead of intersecting. The result is watertight and connected by construction.
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

#include <cstdint>
#include <vector>

#include "TreeMesh.h"
#include "TreeRandom.h"
#include "TreeSpecies.h"
#include "TreeVec3.h"

namespace outshine::Generators {

class TreeGrower {
public:
  /* Delivers `out` normalised: base at y = 0, the SEED RING on the y axis, height 1.
   *
   * `pixelHeightFrac` IS THE MODEL LENGTH OF ONE PIXEL, as a fraction of the tree's own height, at
   * the nearest distance this mesh will be drawn from. It is the only detail knob and it decides two
   * things by the same inequality the impostor's cell size decides its distance by: how many sides a
   * tube gets (silhouette sagitta under half a pixel) and whether a shoot gets a tube at all (a shoot
   * thinner than one pixel is a line, and a line drawn as an eight-sided pipe is 81 % of this mesh —
   * measured, beech: 21 392 of 26 462 triangles have their shortest edge under 2,5 cm). A shoot
   * without a tube still grows and still carries its leaf points, so the crown keeps its shape.
   *
   * 0 grows the mesh the species declares. */
  void Grow(const TreeSpecies &species, TreeMesh &out, float pixelHeightFrac);

  /* How many grow passes the last Grow spent solving the declared DBH, and what it missed by. */
  int Passes() const { return Passes_; }
  float DbhErrorRel() const { return DbhErrorRel_; }
  /* The height the last mesh had BEFORE normalisation, in grower units — what `pixelHeightFrac` was
   * converted with. Reported so the estimate below can be checked against it. */
  float GrowHeight() const { return GrowHeight_; }

private:
  static constexpr int kMaxSides = 16;
  static constexpr int kBranchSides = 8;
  /* The mesh a rank may cost, in vertices; the pixel rule is solved against it. The spawn stop
   * is the hard floor under that solve and must never be what shapes a tree. */
  static constexpr int kVertexBudget = 200000;
  static constexpr int kSpawnCeiling = 400000;

  /* How a shoot's open end is closed. The base ring faces the other way; a live tip runs out to a
   * point; a cut is what a saw or a shear leaves; a break is what wind leaves. */
  enum class RingCap { Base, Point, Cut, Broken };

  struct Tip {
    int Ring[kMaxSides] = {};
    int K = 0;
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
    bool Foliate = true;
    float Roll = 0.0f;
  };
  /* D < 0 marks a triangle. A face is never erased, only marked dead: the indices of its neighbours
   * must survive a branch replacing a wall. */
  struct Face {
    int A, B, C, D;
  };
  /* A SHOOT AS IT IS ASKED FOR, before the silhouette has had its say: where it is anchored, where it
   * points, how thick it leaves the parent and whether it may carry leaves. `Face` < 0 is a shoot with
   * no tube to grow from — it still runs, branches and bears. */
  struct Shoot {
    int Face = -1;
    TreeVec3 Dir, Up;
    float Radius = 0.0f;
    bool Foliate = true;
  };

  int AddVert(TreeVec3 p);
  int AddFace(int a, int b, int c, int d);
  TreeVec3 FaceNormal(int fi) const;
  TreeVec3 FaceCentroid(int fi) const;
  int SidesFor(float radius, int declared) const;

  int ExtrudeCap(Tip &t, TreeVec3 oldDir, float step, float radius, int *ringOut);
  Tip BranchFromFace(int fi, TreeVec3 dir, float radius, float step, int sides);
  void CapRing(const Tip &t, RingCap cap);
  void SpawnLateral(const Tip &t, const TreeSpecies::Growth &g, int first, float roll,
                    int parentStep);
  /* THE ONE PLACE A SHOOT IS ADMITTED. Lateral and terminal fork differ in where they aim and how
   * thick they start; how long a shoot may run inside the silhouette, whether that leaves a branch
   * at all and what the queued tip inherits from its parent is one rule. */
  void SpawnShoot(const Tip &parent, const Shoot &shoot, const TreeSpecies::Growth &g);
  void EmitLeafPoints(TreeMesh &out, TreeVec3 pos, TreeVec3 dir, TreeVec3 up, float radius, int count,
                      float roll);
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
  void GrowOnce(const TreeSpecies::Growth &g, float heightM, TreeMesh &out);
  void Export(TreeMesh &out);
  void NormalizeToUnitHeight(TreeMesh &out, float heightM);

  GrowthForm Form_;
  /* Grower units, all four: the crown's foot and top on the growth axis, the half width the
   * envelope's profile is scaled by, and a hedge section's half run. */
  float CrownBaseY_ = 0.0f, CrownTopY_ = 0.0f, CrownHalfWidth_ = 0.0f, HalfRunX_ = 0.0f;
  std::vector<TreeVec3> Verts_;
  std::vector<Face> Faces_;
  std::vector<uint8_t> Dead_;
  std::vector<Tip> Queue_;
  std::vector<TreeVec3> TrunkProfile_; /* X = height above the seed ring, Y = radius, both grower units */
  std::vector<TreeVec3> Normals_;
  TreeRandom Rng_{1};
  int Passes_ = 0;
  float DbhErrorRel_ = 0.0f;
  float PixelGrow_ = 0.0f;
  float GrowHeight_ = 0.0f;
};

} // namespace outshine::Generators
#endif

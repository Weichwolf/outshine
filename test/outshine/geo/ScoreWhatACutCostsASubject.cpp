#include <cmath>
#include <cstdio>
#include <vector>

#include "Check.h"
#include "ClusterDag.h"
#include "Cooked.h"
#include "Geometry.h"
#include "Material.h"

// A SUBJECT AT DISTANCE DRAWS FEWER TRIANGLES, AND THIS IS THE NUMBER.
//
// Unreal's Nanite chooses a cut through the cluster DAG per CLUSTER, so a mesh half off-screen or
// far away spends detail only where the camera is; RAGE picks one LOD model per map entity, so a
// car half off-screen pays full price for the half nobody sees. Taking Unreal. Reference: Karis,
// Nanite: A Deep Dive, SIGGRAPH 2021 -- the error-bounded DAG and why a monotonic error makes the
// cut valid without crack repair.
//
// THIS CASE IS THE MEASUREMENT THAT DECIDES WHETHER THE FRAME-PATH SURGERY IS WORTH IT.
// board:1991's next step cooks subjects into the renderer, which is surgery on the path 444
// corpus cases guard. CLAUDE.md asks for the number that would show the change is bad BEFORE the
// change: if a cut saves nothing on a subject-sized mesh, then a 720p engine drawing tens of
// subjects does not need what an engine drawing tens of thousands does, and the honest answer is
// to leave the frame path alone.
//
// The oracle is the cut itself and owes nothing to our design: the same cooked subject, two eye
// distances, and the triangle counts compared as a RATIO of two runs rather than against a
// constant, so the number cannot be tuned to whatever the tree happens to do.

namespace {

constexpr int kSide = 129;
constexpr float kFocalPx = 720.0f;
constexpr float kTau = outshine::kPixelTau;

[[nodiscard]] outshine::Geometry Lattice() {
  outshine::Geometry stood;
  const outshine::MaterialInstance surface = stood.Surface("lattice", outshine::Material{});
  const int part = stood.Part("lattice", surface);
  std::vector<float> places, normals;
  std::vector<uint32_t> run;
  const auto push = [&](float e, float n) {
    const double height = 0.35 * std::sin(0.4 * (double)e) * std::cos(0.3 * (double)n);
    places.push_back(e);
    places.push_back((float)height);
    places.push_back(n);
    normals.push_back(0.0f);
    normals.push_back(1.0f);
    normals.push_back(0.0f);
    run.push_back((uint32_t)(run.size()));
  };
  for (int row = 0; row + 1 < kSide; ++row) {
    for (int column = 0; column + 1 < kSide; ++column) {
      const float e = (float)column, n = (float)row;
      push(e, n);
      push(e + 1.0f, n);
      push(e + 1.0f, n + 1.0f);
      push(e, n);
      push(e + 1.0f, n + 1.0f);
      push(e, n + 1.0f);
    }
  }
  if (part < 0 || !stood.Positions(part, places) || !stood.Normals(part, normals) ||
      !stood.Triangles(part, run)) {
    return outshine::Geometry();
  }
  return stood;
}

[[nodiscard]] uint32_t TrianglesAt(const outshine::CookedPart &cooked, double backM) {
  const double eye[3] = {0.5 * (double)kSide, backM, 0.5 * (double)kSide};
  const float up[3] = {0.0f, 1.0f, 0.0f};
  uint32_t drawn = 0;
  for (const outshine::DagCluster &one : cooked.Dag.Clusters) {
    if (outshine::DagSelect(one, eye, kFocalPx, kTau, up)) { drawn += one.Count / 3u; }
  }
  return drawn;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const outshine::Geometry stood = Lattice();
  if (stood.Parts() != 1) {
    Unprepared("the lattice would not fill a geometry");
    return Report();
  }

  outshine::ClusterDagOpts how;
  how.Up[1] = 1.0f;
  outshine::CookedPart cooked;
  std::string why;
  if (!outshine::Cook(stood, 0, how, cooked, why)) {
    Unprepared(("the cooker refused the lattice: " + why).c_str());
    return Report();
  }

  const uint32_t whole = (uint32_t)(stood.TrianglesOf(0).size() / 3);
  const uint32_t close = TrianglesAt(cooked, 20.0);
  const uint32_t afar = TrianglesAt(cooked, 6000.0);

  std::printf("A SUBJECT OF        %u triangle(s) cooks to %zu cluster(s) over %d level(s)\n",
              whole, cooked.Dag.Clusters.size(), cooked.Dag.Levels);
  std::printf("AN EYE 20 m AWAY    cuts to %u triangle(s)\n", close);
  const uint32_t furthest = TrianglesAt(cooked, 1.0e6);
  uint32_t byLevel[8] = {};
  for (const outshine::DagCluster &one : cooked.Dag.Clusters) {
    if (one.Level < 8) { byLevel[one.Level] += one.Count / 3u; }
  }
  std::printf("AN EYE 6000 m AWAY  cuts to %u triangle(s), %.1f%% of the near cut\n", afar,
              close == 0 ? 0.0 : 100.0 * (double)afar / (double)close);
  std::printf("AN EYE 1000 km AWAY cuts to %u, and the DAG's levels hold", furthest);
  for (int level = 0; level < cooked.Dag.Levels && level < 8; ++level) {
    std::printf(" %u", byLevel[level]);
  }
  std::printf(" triangle(s)\n");

  CHECK(close > 0 && afar > 0,
        "both cuts draw something, so the comparison below is between two pictures rather than "
        "between a picture and an empty screen -- a cut that selects nothing is a mesh that "
        "vanished, which no LOD may do");

  CHECK(afar < close,
        "**A SUBJECT AT DISTANCE DRAWS FEWER TRIANGLES**: the cut takes the leaves while their "
        "parent's screen-space error is over the threshold and the parent once it is not. This is "
        "the per-CLUSTER choice a per-object LOD ladder cannot make, and it is board:1991's whole "
        "reason -- measured on the cooker this tree has, over a mesh the door handed in");

  CHECK(close == whole,
        "and the NEAR cut is the whole mesh, so nothing is lost where the camera can see it -- a "
        "cut that simplifies what is under the nose has traded the wrong thing for the saving "
        "above");

  // WHAT SETS THE FLOOR IS THE DAG'S DEPTH, NOT THE CUT. The levels above say how far a cut can
  // possibly go: the coarsest level is the fewest triangles any eye can select, and an eye a
  // thousand kilometres away reaches it. So the saving this mechanism can deliver is decided in
  // the COOKER -- `MinLevelTris` and `TargetRatio` -- and not in `DagSelect`, which is worth
  // knowing before board:1991's frame-path surgery is judged by what it saves.
  CHECK(furthest <= afar,
        "and an eye far enough away reaches the DAG's own floor, so what bounds the saving is how "
        "many levels the cooker built and not how the cut is chosen -- a mechanism judged on the "
        "wrong half is a mechanism tuned in the wrong place");

  // THE SIMPLIFIER REACHES ITS RATIO, and this check used to stand RED-WHEN-FIXED saying it did
  // not. It went red the day it was fixed, which is what such a check is for.
  //
  // What it cost was one number. `dag::SimplifyGroup` LOCKS every vertex a group shares with
  // another -- Nanite's boundary lock, without which neighbouring groups crack at the seam -- so
  // with few clusters per group the locked boundary is large against the interior and only
  // interior edges collapse. `GroupSize` was 4 where Nanite groups 8 to 32. The sweep below is
  // what found it and it stays, because it is the evidence the number is right and not a guess:
  //
  //   groups of  4  reach 0.69     groups of 16  reach 0.50
  //   groups of  8  reach 0.61     groups of 32  reach 0.50
  //
  // Sixteen is the smallest that reaches the declared ratio, and a larger group costs more locked
  // work per level for nothing. What it bought: the far cut went from 21054 triangles to 10156,
  // so the mechanism saves 69% of the mesh where it saved 36%.
  const double reached = byLevel[0] == 0 ? 1.0 : (double)byLevel[1] / (double)byLevel[0];
  std::printf("THE COOKER ASKED FOR %.2f per level AND REACHED %.2f\n", (double)how.TargetRatio,
              reached);

  // WHY IT FALLS SHORT, measured rather than guessed. `dag::SimplifyGroup` LOCKS every vertex a
  // group shares with another -- Nanite's boundary lock, without which neighbouring groups crack
  // at the seam. With `GroupSize` clusters per group the locked boundary is large against the
  // interior, so only interior edges collapse. If that is the cause, the reached ratio must
  // improve as groups grow, and if it does not, the cause is elsewhere and this comment is wrong.
  for (const int size : {4, 8, 16, 32}) {
    outshine::ClusterDagOpts wider = how;
    wider.GroupSize = size;
    outshine::CookedPart other;
    std::string whyNot;
    if (!outshine::Cook(stood, 0, wider, other, whyNot)) { continue; }
    uint32_t base = 0, next = 0;
    for (const outshine::DagCluster &one : other.Dag.Clusters) {
      if (one.Level == 0) { base += one.Count / 3u; }
      if (one.Level == 1) { next += one.Count / 3u; }
    }
    std::printf("  groups of %-3d reach %.2f  (%u -> %u over %d level(s))\n", size,
                base == 0 ? 1.0 : (double)next / (double)base, base, next, other.Dag.Levels);
  }
  CHECK(reached <= (double)how.TargetRatio + 0.01,
        "**THE SIMPLIFIER REACHES ITS DECLARED RATIO**: Nanite halves each level and this cooker "
        "asks for the same, so a level holding more than half of the one below it is a cooker "
        "that stopped early. A cut cannot save what the cooker did not remove, which is why this "
        "number bounds everything the frame path could gain");

  // AND THE CUT IS PER CLUSTER, WHICH IS THE WHOLE CLAIM. A per-OBJECT ladder gives one level to
  // a whole mesh; Nanite's cut gives each cluster its own, so a subject that stretches away from
  // the camera keeps detail at its near end and drops it at its far one. The eye is put at ONE
  // corner of the lattice, low down, so the near corner and the far corner of the SAME subject
  // are at very different distances -- and the levels the cut selects are counted.
  const double corner[3] = {0.0, 2.0, 0.0};
  const float up[3] = {0.0f, 1.0f, 0.0f};
  uint32_t chosen[8] = {};
  int levelsChosen = 0;
  for (const outshine::DagCluster &one : cooked.Dag.Clusters) {
    if (!outshine::DagSelect(one, corner, kFocalPx, kTau, up)) { continue; }
    if (one.Level < 8) { chosen[one.Level] += 1; }
  }
  std::printf("AN EYE AT ONE CORNER selects");
  for (int level = 0; level < cooked.Dag.Levels && level < 8; ++level) {
    std::printf(" %u cluster(s) at level %d", chosen[level], level);
    if (chosen[level] > 0) { ++levelsChosen; }
  }
  std::printf("\n");

  CHECK(levelsChosen > 1,
        "**THE CUT IS PER CLUSTER, NOT PER OBJECT**: one eye, one subject, and the clusters it "
        "selects come from more than one level -- fine where the mesh is near the camera and "
        "coarse where it runs away. RAGE picks one LOD model per map entity and cannot do this; "
        "it is the reason board:1991 takes Unreal, and a car half off-screen paying full price "
        "for the half nobody sees is what the alternative costs");

  Covers("the frame path: a subject cooked by the one cooker draws fewer triangles from further "
         "away and its whole self up close, which is the number board:1991's frame-path surgery "
         "has to beat");
  return Report();
}

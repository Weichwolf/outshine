#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

#include "Check.h"
#include "ClusterCook.h"

// WHAT A CUT MUST BE TRUE OF, WHATEVER THE CUT IS. This states nothing about the cut being GOOD --
// a better clustering is coming and this case has to survive it. It states the two things a wrong
// cut breaks: a triangle that lands in no cluster is a HOLE in the picture, and one that lands in
// two is drawn twice and paid for twice. Both are invisible in a count and obvious in a set.
//
// The sphere check is the other half: the device rejects a cluster by its sphere, so a sphere that
// does not contain its own vertices rejects geometry that was on screen. That is a containment
// predicate and its truth does not depend on our design, which is what makes it worth writing here
// while the design is still moving.

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // A GRID OF QUADS, because a cut by locality has something to do with it and the expected counts
  // are arithmetic rather than a measurement.
  constexpr int kSide = 16;
  std::vector<float> positions;
  for (int row = 0; row <= kSide; ++row) {
    for (int column = 0; column <= kSide; ++column) {
      positions.push_back((float)column);
      positions.push_back(row % 2 == 0 ? 0.0f : 0.5f);
      positions.push_back((float)row);
    }
  }
  std::vector<uint32_t> indices;
  const auto at = [](int row, int column) { return (uint32_t)(row * (kSide + 1) + column); };
  for (int row = 0; row < kSide; ++row) {
    for (int column = 0; column < kSide; ++column) {
      indices.push_back(at(row, column));
      indices.push_back(at(row + 1, column));
      indices.push_back(at(row, column + 1));
      indices.push_back(at(row, column + 1));
      indices.push_back(at(row + 1, column));
      indices.push_back(at(row + 1, column + 1));
    }
  }
  const size_t triangles = indices.size() / 3;

  constexpr uint32_t kMostTriangles = 64;
  const outshine::Cooked cut = outshine::CookClusters(positions, indices, kMostTriangles);

  Note("triangles handed in", (double)triangles, "triangles");
  Note("clusters the cut made", (double)cut.Clusters.size(), "clusters");
  Note("triangles the clusters carry", (double)(cut.Index.size() / 3), "triangles");

  CHECK(cut.Index.size() == indices.size(),
        "**A CUT KEEPS EVERY TRIANGLE**: the clusters together carry exactly what was handed in, "
        "because a triangle in no cluster is a hole in the picture and one in two is drawn twice");

  std::multiset<std::vector<uint32_t>> handed, carried;
  for (size_t triangle = 0; triangle < triangles; ++triangle) {
    handed.insert({indices[triangle * 3], indices[triangle * 3 + 1], indices[triangle * 3 + 2]});
  }
  for (size_t triangle = 0; triangle * 3 + 2 < cut.Index.size(); ++triangle) {
    carried.insert(
        {cut.Index[triangle * 3], cut.Index[triangle * 3 + 1], cut.Index[triangle * 3 + 2]});
  }
  CHECK(handed == carried,
        "**AND IT KEEPS THEM ONCE EACH, AS THEY WERE WOUND**: the cut REORDERS triangles and must "
        "not rewrite one. A corner permuted inside a triangle flips its winding, and a flipped "
        "triangle faces away from the light that was lighting it");

  size_t clustersCovering = 0;
  double worstOutsideM = 0.0;
  for (const outshine::DagCluster &cluster : cut.Clusters) {
    bool covers = true;
    for (uint32_t step = 0; step < cluster.Count; ++step) {
      const uint32_t index = cut.Index[cluster.First + step];
      double away = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        const double held =
            (double)positions[(size_t)index * 3 + (size_t)axis] - (double)cluster.SelfCenter[axis];
        away += held * held;
      }
      const double outside = std::sqrt(away) - (double)cluster.SelfRadius;
      if (outside > worstOutsideM) { worstOutsideM = outside; }
      if (outside > 1.0e-4) { covers = false; }
    }
    if (covers) { ++clustersCovering; }
  }
  Note("the furthest a vertex sits outside its own cluster's sphere", worstOutsideM, "m");
  CHECK(clustersCovering == cut.Clusters.size(),
        "**AND EVERY CLUSTER'S SPHERE HOLDS ITS OWN VERTICES**: the device rejects a cluster by "
        "that sphere, so one that does not contain what it stands for rejects geometry that was on "
        "screen -- and a sphere is the only thing a compute cull has to go on");

  // AND THE DAG ON TOP OF THAT CUT. A parent error is a BOUND on what the coarser level
  // misrepresents, and `DagSelect` trusts it in both directions: too large keeps a cluster that
  // could have been dropped, too small drops one that was still needed. So the claim is that the
  // number the cooker states is not smaller than the displacement it actually caused.
  const outshine::Cooked dag = outshine::CookDag(positions, indices, kMostTriangles, 1);
  size_t leaves = 0, above = 0;
  float statedErrM = 0.0f;
  for (const outshine::DagCluster &cluster : dag.Clusters) {
    if (cluster.Level == 0) {
      ++leaves;
      statedErrM = cluster.ParentErr;
    } else {
      ++above;
    }
  }
  Note("clusters at the leaves", (double)leaves, "clusters");
  Note("clusters one level above", (double)above, "clusters");
  Note("the parent error they were given", (double)statedErrM, "m");
  Note("vertices the cooker made for that level",
       (double)(dag.PositionsM.size() / 3 - dag.FirstOwnVertex),
       "vertices");

  CHECK(above > 0 && statedErrM > 0.0f && statedErrM < outshine::kDagRootErr,
        "**A DAG HAS A LEVEL ABOVE ITS LEAVES, AND THE LEAVES KNOW ITS ERROR**: without one, "
        "`DagSelect`'s parent test passes for every cluster at every distance and the cut is a "
        "list -- enough to cull with, not enough to choose a level with");

  double worstMovedM = 0.0;
  for (const outshine::DagCluster &cluster : dag.Clusters) {
    if (cluster.Level != 0) { continue; }
    for (uint32_t step = 0; step < cluster.Count; ++step) {
      const uint32_t index = dag.Index[cluster.First + step];
      double nearest = 1.0e300;
      for (size_t made = dag.FirstOwnVertex; made * 3 + 2 < dag.PositionsM.size(); ++made) {
        double away = 0.0;
        for (int axis = 0; axis < 3; ++axis) {
          const double held = (double)dag.PositionsM[(size_t)index * 3 + (size_t)axis] -
                              (double)dag.PositionsM[made * 3 + (size_t)axis];
          away += held * held;
        }
        if (away < nearest) { nearest = away; }
      }
      const double moved = std::sqrt(nearest);
      if (moved > worstMovedM) { worstMovedM = moved; }
    }
  }
  Note("the furthest a leaf vertex sits from the nearest vertex the level above made",
       worstMovedM,
       "m");
  CHECK(worstMovedM <= (double)statedErrM + 1.0e-4,
        "**AND THE STATED ERROR IS NOT SMALLER THAN THE DISPLACEMENT IT CAUSED**: a bound that "
        "under-reports is worse than no bound, because the selection believes it and drops a "
        "cluster whose replacement is further off than it was told");

  Covers(
      "the cooker's cut: every triangle lands in exactly one cluster with its winding intact, "
      "and every cluster's bounding sphere contains the vertices it was measured over; and its "
      "DAG: a level stands above the leaves and the parent error it states bounds the displacement "
      "that level actually caused");
  return Report();
}

#include <algorithm>
#include "Check.h"
#include "TreeMesher.h"

namespace {
float Rightmost(const outshine::Generators::TreeMesh &mesh) {
  float most = 0;
  for (size_t at = 0; at < mesh.BarkVerts.size(); at += outshine::Generators::TreeMesh::kBarkFloats) {
    most = std::max(most, mesh.BarkVerts[at]);
  }
  return most;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  TreeSkeleton plant;
  plant.Nodes = {
      {.Pos={{0,0,0}}, .Dir={{0,1,0}}, .Up={{0,0,1}}, .Radius=0.03f},
      {.Pos={{0,1,0}}, .Dir={{0,1,0}}, .Up={{0,0,1}}, .Radius=0.02f},
      {.Pos={{0,0.5f,0}}, .Dir={{1,0,0}}, .Up={{0,0,1}}, .Radius=0.0001f},
      {.Pos={{0.3f,0.5f,0}}, .Dir={{1,0,0}}, .Up={{0,0,1}}, .Radius=0.0001f}};
  plant.Shoots = {
      {.First=0, .Count=2, .Sides=8, .Reach=1.1f},
      {.Parent=0, .ParentNode=1, .First=2, .Count=2, .Sides=8, .Reach=0.31f}};
  TreeMesher mesher;
  TreeMesh coarse, fine, unbounded;
  mesher.Draw(plant, 0.001f, coarse);
  mesher.Draw(plant, 0.00001f, fine);
  mesher.Draw(plant, 0, unbounded);
  CHECK(!coarse.BarkIdx.empty(), "the visible trunk survives coarse selection");
  CHECK(Rightmost(coarse) < 0.1f, "a long branch below one pixel in diameter has no explicit coarse mesh");
  CHECK(Rightmost(fine) > 0.29f, "moving close enough restores the full branch length");
  CHECK(Rightmost(unbounded) > 0.29f, "negative control: disabling the pixel threshold keeps the thin branch");
  plant.Nodes[2].Radius = plant.Nodes[3].Radius = 0.005f;
  TreeMesh thick;
  mesher.Draw(plant, 0.001f, thick);
  CHECK(Rightmost(thick) > 0.29f, "a resolvable branch at the same distance must remain");
  return Report();
}

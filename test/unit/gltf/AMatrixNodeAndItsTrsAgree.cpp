#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"

using outshine::Gltf::Document;
using outshine::Gltf::Transform;
using outshine::Test::Glb;

namespace {

const double kHalfTurnRoot = 0.70710678118654752440;

std::string Json() {

  char digits[32];
  std::snprintf(digits, sizeof digits, "%.17g", kHalfTurnRoot);
  const std::string half = digits;
  return std::string(R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [
    { "name": "parent", "translation": [1, 2, 3], "rotation": [)") +
         half + R"(, 0, 0, )" + half + R"(], "children": [1, 2] },
    { "name": "trs", "translation": [5, -6, 7], "rotation": [0, 0, )" + half + ", " + half +
         R"(], "scale": [2, 3, 4] },
    { "name": "matrix", "matrix": [0, 2, 0, 0, -3, 0, 0, 0, 0, 0, 4, 0, 5, -6, 7, 1] }
  ]
})";
}

const double kExpectedWorld[16] = {0, 0, 2, 0, -3, 0, 0, 0, 0, -4, 0, 0, 6, -5, -3, 1};

}

int main() {
  using namespace outshine::Test;

  const std::string json = Json();
  const std::vector<uint8_t> glb = Glb(json, {});
  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "hierarchy.glb");
  CHECK(read, "a GLB with no binary chunk and a node hierarchy is read");
  if (!read) {
    std::printf("       %s\n%s\n", document.Error().c_str(), json.c_str());
    return Report();
  }
  CHECK(document.Nodes().size() == 3, "three nodes cross");
  CHECK(!document.Nodes()[1].HasMatrix && document.Nodes()[2].HasMatrix,
        "one child is a TRS triple and the other is a matrix");

  Transform fromTrs, fromMatrix;
  CHECK(document.WorldTransform(1, fromTrs), "the TRS child has a world transform");
  CHECK(document.WorldTransform(2, fromMatrix), "the matrix child has a world transform");

  for (int i = 0; i < 16; ++i) {
    CHECK_NEAR(fromTrs.M[i], kExpectedWorld[i], 1e-12, "unit",
               "the TRS child's world transform is the one a pencil derives");
    CHECK_NEAR(fromMatrix.M[i], kExpectedWorld[i], 1e-12, "unit",
               "the matrix child's world transform is the same one");
  }

  const double vertex[3] = {1.0, 1.0, 1.0};
  double byTrs[3] = {0, 0, 0};
  double byMatrix[3] = {0, 0, 0};
  fromTrs.Point(vertex, byTrs);
  fromMatrix.Point(vertex, byMatrix);
  const double expected[3] = {3.0, -9.0, -1.0};
  double worst = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    CHECK_NEAR(byTrs[axis], expected[axis], 1e-12, "m",
               "a vertex placed through the TRS child lands where the derivation says");
    const double off = std::fabs(byTrs[axis] - byMatrix[axis]);
    worst = (off > worst) ? off : worst;
  }
  CHECK_NEAR(worst, 0.0, 1e-12, "m",
             "the two spellings place the same vertex in the same place");
  Note("worst disagreement between a matrix node and its TRS twin", worst, "m");
  Covers("I.53 nodes: hierarchy, TRS and matrix transforms, scenes");
  return Report();
}

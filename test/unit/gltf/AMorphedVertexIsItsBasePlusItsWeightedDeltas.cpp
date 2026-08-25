#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"
#include "Subject.h"

using outshine::Gltf::Document;
using outshine::Gltf::Subject;

namespace {

std::vector<uint8_t> Binary() {
  using outshine::Test::Append;
  std::vector<uint8_t> out;

  const float base[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float value : base) { Append(out, value); }

  const float first[9] = {0, 0, 0, 2, 0, 0, 0, 0, 0};
  for (float value : first) { Append(out, value); }

  const float second[9] = {0, 0, 0, 0, 0, 0, 0, 4, 0};
  for (float value : second) { Append(out, value); }

  const uint16_t run[3] = {0, 1, 2};
  for (uint16_t value : run) { Append(out, value); }
  return out;
}

const char *const kJson = R"({
  "asset": { "version": "2.0" },
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "name": "morphed", "mesh": 0, "scale": [10, 10, 10] } ],
  "meshes": [ {
    "weights": [1.5, -1.0],
    "primitives": [ {
      "attributes": { "POSITION": 0 },
      "indices": 3,
      "targets": [ { "POSITION": 1 }, { "POSITION": 2 } ] } ] } ],
  "buffers": [ { "byteLength": 114 } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,   "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36,  "byteLength": 36 },
    { "buffer": 0, "byteOffset": 72,  "byteLength": 36 },
    { "buffer": 0, "byteOffset": 108, "byteLength": 6 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
      "min": [0, 0, 0], "max": [1, 1, 0] },
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC3" },
    { "bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ]
})";

bool Refuses(const char *json, const std::string &naming) {
  const std::vector<uint8_t> glb = outshine::Test::Glb(json, Binary());
  Document document;
  if (document.Read({glb.data(), glb.size()}, "morph.glb")) { return false; }
  return document.Error().find(naming) != std::string::npos;
}

}

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> glb = Glb(kJson, Binary());
  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "morph.glb");
  CHECK(read, "a file carrying morph targets is read");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }

  CHECK(document.Meshes()[0].Weights.size() == 2 && document.Meshes()[0].Weights[0] == 1.5 &&
            document.Meshes()[0].Weights[1] == -1.0,
        "mesh.weights crosses as declared, including the value outside [0, 1] that a morph allows");
  CHECK(document.Meshes()[0].Primitives[0].Targets.size() == 2,
        "both morph targets reach the primitive, in the file's own order");
  CHECK(document.MorphWeightsCount(0) == 2 && document.MorphWeightsTotal() == 2,
        "the document states one flat weight layout, and it is the mesh's target count");

  Subject subject;
  const bool flat = subject.Build(document);
  CHECK(flat, "the morphed subject flattens");
  if (!flat) {
    std::printf("       %s\n", subject.Error().c_str());
    return Report();
  }

  const std::vector<double> &at = subject.PositionsM();
  CHECK(subject.VertexCount() == 3, "three vertices reach the flatten");
  if (subject.VertexCount() != 3) { return Report(); }

  const double want[9] = {0, 0, 0, 40, 0, 0, 0, -30, 0};
  for (size_t vertex = 0; vertex < 3; ++vertex) {
    for (size_t axis = 0; axis < 3; ++axis) {
      CHECK_NEAR(at[vertex * 3 + axis], want[vertex * 3 + axis], 1e-9, "m",
                 "the morphed vertex is its base plus its weighted deltas, then its node");
    }
  }

  CHECK_NEAR(at[3], 40.0, 1e-9, "m",
             "vertex 1 is (1 + 1.5*2) * 10, so the weight was neither clamped to 1 nor normalised "
             "against its neighbour");
  CHECK_NEAR(at[7], -30.0, 1e-9, "m",
             "vertex 2 is (1 + -1*4) * 10, so a NEGATIVE weight displaces against the target and is "
             "not treated as zero");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{"COLOR_0":1}]}]}],
        "buffers":[{"byteLength":114}],"bufferViews":[{"buffer":0,"byteLength":114}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,1]},
                     {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,1]}]})",
                "POSITION, NORMAL or TANGENT"),
        "a target displacing a semantic glTF does not allow in one is refused by name");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{"NORMAL":1}]}]}],
        "buffers":[{"byteLength":114}],"bufferViews":[{"buffer":0,"byteLength":114}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,1]},
                     {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,1]}]})",
                "the primitive carries none"),
        "a target displacing a semantic the primitive does not carry is refused, because the delta "
        "has nothing to displace");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0}],
        "meshes":[{"weights":[1.0],"primitives":[{"attributes":{"POSITION":0},
                   "targets":[{"POSITION":1},{"POSITION":1}]}]}],
        "buffers":[{"byteLength":114}],"bufferViews":[{"buffer":0,"byteLength":114}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,1]},
                     {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,1]}]})",
                "weights over"),
        "one weight against two targets is refused, because the pairing is positional");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{"POSITION":1}]}]}],
        "buffers":[{"byteLength":114}],"bufferViews":[{"buffer":0,"byteLength":114}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,1]},
                     {"bufferView":0,"componentType":5126,"count":2,"type":"VEC3","min":[0,0,0],"max":[1,1,1]}]})",
                "deltas over"),
        "a target whose accessor is shorter than the base attribute is refused by name");

  Note("morph targets applied", 2.0, "over 3 vertices");
  Covers("I.76 morph targets: mesh weights, per-target deltas, and the order they apply in");
  return Report();
}

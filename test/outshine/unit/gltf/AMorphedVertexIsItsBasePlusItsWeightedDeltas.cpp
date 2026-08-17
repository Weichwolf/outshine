/* MORPH TARGETS, AND THE THREE THINGS ABOUT THEM THAT ARE EASY TO GET WRONG QUIETLY (board:1203).
 *
 * ONE: the weights are NOT normalised, and must not be. A skin's weights have a `SHOULD` behind them;
 * a morph weight set has no constraint at all, so 0.5 and 0.5 do not make a half-way blend of two
 * targets -- they make half of each, added. This fixture uses weights that sum to 1.5 and to -1, both
 * of which the format allows and both of which a renormalising implementation would silently repair.
 *
 * TWO: a tangent delta is VEC3 against a VEC4 base, because `w` is the bitangent's SIGN. A blend that
 * touched it would produce a fourth component naming no handedness at all.
 *
 * THREE: the deltas apply in the mesh's OWN space, before the node transform -- so the node here
 * carries a scale of 10, and a delta that had been put through it would arrive ten times too large.
 */
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
  /* POSITION, three vertices, byte 0 */
  const float base[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (float value : base) { Append(out, value); }
  /* target 0's POSITION delta, byte 36: moves only vertex 1, by +2 in x */
  const float first[9] = {0, 0, 0, 2, 0, 0, 0, 0, 0};
  for (float value : first) { Append(out, value); }
  /* target 1's POSITION delta, byte 72: moves only vertex 2, by +4 in y */
  const float second[9] = {0, 0, 0, 0, 0, 0, 0, 4, 0};
  for (float value : second) { Append(out, value); }
  /* indices, byte 108 */
  const uint16_t run[3] = {0, 1, 2};
  for (uint16_t value : run) { Append(out, value); }
  return out;
}

/* `mesh.weights` is 1.5 and -1: neither is in [0, 1] and they do not sum to one, which the format
 * permits for a morph and which is the whole point of stating them here. Vertex 1 therefore moves
 * 1.5 * 2 = 3 in x, and vertex 2 moves -1 * 4 = -4 in y. The node scales by 10 AFTER that, so the
 * drawn positions are 40 and -40 -- and a delta applied in the node's space would give 30 and -40
 * against a base already at 10, which is a different picture rather than a different scale. */
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
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3" },
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

} // namespace

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

  /* v0 is displaced by neither target and only scaled; v1 by target 0 at weight 1.5; v2 by target 1
   * at weight -1, which moves it the OTHER way and is what a clamp to [0, 1] would erase. */
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
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
                     {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}]})",
                "POSITION, NORMAL or TANGENT"),
        "a target displacing a semantic glTF does not allow in one is refused by name");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{"NORMAL":1}]}]}],
        "buffers":[{"byteLength":114}],"bufferViews":[{"buffer":0,"byteLength":114}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
                     {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}]})",
                "the primitive carries none"),
        "a target displacing a semantic the primitive does not carry is refused, because the delta "
        "has nothing to displace");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0}],
        "meshes":[{"weights":[1.0],"primitives":[{"attributes":{"POSITION":0},
                   "targets":[{"POSITION":1},{"POSITION":1}]}]}],
        "buffers":[{"byteLength":114}],"bufferViews":[{"buffer":0,"byteLength":114}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
                     {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}]})",
                "weights over"),
        "one weight against two targets is refused, because the pairing is positional");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{"POSITION":1}]}]}],
        "buffers":[{"byteLength":114}],"bufferViews":[{"buffer":0,"byteLength":114}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
                     {"bufferView":0,"componentType":5126,"count":2,"type":"VEC3"}]})",
                "deltas over"),
        "a target whose accessor is shorter than the base attribute is refused by name");

  Note("morph targets applied", 2.0, "over 3 vertices");
  Covers("I.26.6 morph targets: mesh weights, per-target deltas, and the order they apply in");
  return Report();
}

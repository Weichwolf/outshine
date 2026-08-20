/* LINEAR BLEND SKINNING, AND THE CLAUSE THAT IS EASIEST TO GET WRONG SILENTLY (board:1200).
 *
 * glTF states that the transform of the node instantiating a skinned mesh MUST BE IGNORED -- only the
 * joints move the vertices. A flatten that applied the node transform as well renders a subject that
 * looks posed, sits somewhere else, and fails no assertion anybody wrote. So the skinned node here
 * carries a translation of 100 m in y, and no vertex may show it.
 *
 * THE FIXTURE IS ARRANGED SO EVERY NUMBER IS HAND-CHECKABLE AND EACH ONE FAILS DIFFERENTLY:
 *
 *   joint A  the identity                          inverse bind: identity
 *   joint B  a quarter turn about +Z               inverse bind: translate (2, 0, 0)
 *
 * so joint B's matrix is `R * T`, which sends (x, y, z) to (-y, x + 2, z) -- and the ORDER matters,
 * because `T * R` would send it to (-y + 2, x, z). Translations commute and rotations do not, which is
 * why the bind pose here is a rotation rather than the offset that would have hidden a swap.
 *
 *   v0 = (0, 0, 0)   weights (1, 0)     -> (0, 0, 0)     the joint that does nothing
 *   v1 = (1, 0, 0)   weights (0, 1)     -> (0, 3, 0)     one joint, fully
 *   v2 = (0, 0, 1)   weights (.5, .5)   -> (0, 1, 1)     the blend, and it is the mean of the two
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

/* A quarter turn about +Z as glTF stores a quaternion: x, y, z, w. */
const char *const kQuarterTurnZ = "0, 0, 0.7071067811865476, 0.7071067811865476";

std::vector<uint8_t> Binary() {
  using outshine::Test::Append;
  std::vector<uint8_t> out;
  /* POSITION, three points, at byte 0 */
  const float points[9] = {0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (float value : points) { Append(out, value); }
  /* JOINTS_0, VEC4 of unsigned short, at byte 36 */
  const uint16_t bones[12] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0};
  for (uint16_t value : bones) { Append(out, value); }
  /* WEIGHTS_0, VEC4 of float, at byte 60 */
  const float shares[12] = {1, 0, 0, 0, 1, 0, 0, 0, 0.5f, 0.5f, 0, 0};
  for (float value : shares) { Append(out, value); }
  /* inverseBindMatrices, two MAT4, at byte 108: identity, then translate (2, 0, 0) */
  const float bind[32] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                          1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1};
  for (float value : bind) { Append(out, value); }
  /* indices, at byte 236 */
  const uint16_t run[3] = {0, 1, 2};
  for (uint16_t value : run) { Append(out, value); }
  return out;
}

std::string Json() {
  return std::string(R"({
  "asset": { "version": "2.0" },
  "scenes": [ { "nodes": [0] } ],
  "nodes": [
    { "name": "rig", "children": [1, 2, 3] },
    { "name": "restJoint" },
    { "name": "turnJoint", "rotation": [)") + kQuarterTurnZ + R"(] },
    { "name": "skinned", "mesh": 0, "skin": 0, "translation": [0, 100, 0] }
  ],
  "meshes": [ { "primitives": [ {
    "attributes": { "POSITION": 0, "JOINTS_0": 1, "WEIGHTS_0": 2 }, "indices": 4 } ] } ],
  "buffers": [ { "byteLength": 244 } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,   "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36,  "byteLength": 24 },
    { "buffer": 0, "byteOffset": 60,  "byteLength": 48 },
    { "buffer": 0, "byteOffset": 108, "byteLength": 128 },
    { "buffer": 0, "byteOffset": 236, "byteLength": 6 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3" },
    { "bufferView": 1, "componentType": 5123, "count": 3, "type": "VEC4" },
    { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4" },
    { "bufferView": 3, "componentType": 5126, "count": 2, "type": "MAT4" },
    { "bufferView": 4, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ],
  "skins": [ { "joints": [1, 2], "inverseBindMatrices": 3 } ]
})";
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::string json = Json();
  const std::vector<uint8_t> binary = Binary();
  CHECK(binary.size() == 242, "the fixture's binary chunk is the length its views address");
  const std::vector<uint8_t> glb = Glb(json, binary);

  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "skinned.glb");
  CHECK(read, "the skinned file is read");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }

  Subject subject;
  const bool flat = subject.Build(document);
  CHECK(flat, "the skinned subject flattens");
  if (!flat) {
    std::printf("       %s\n", subject.Error().c_str());
    return Report();
  }

  CHECK(subject.VertexCount() == 3, "three vertices reach the flatten");
  if (subject.VertexCount() != 3) { return Report(); }
  const std::vector<double> &at = subject.PositionsM();

  const double want[9] = {0, 0, 0, 0, 3, 0, 0, 1, 1};
  static const char *const kAxis[3] = {"x", "y", "z"};
  for (size_t vertex = 0; vertex < 3; ++vertex) {
    for (size_t axis = 0; axis < 3; ++axis) {
      CHECK_NEAR(at[vertex * 3 + axis], want[vertex * 3 + axis], 1e-9, "m",
                 "the skinned vertex lands where its joints put it");
      (void)kAxis;
    }
  }

  /* THE CLAUSE THIS CASE EXISTS FOR, stated as its own check so a failure names it rather than
   * appearing as three coordinates being wrong by the same amount. */
  bool carriedTheNode = false;
  for (size_t vertex = 0; vertex < 3; ++vertex) { carriedTheNode = carriedTheNode || at[vertex * 3 + 1] > 50.0; }
  CHECK(!carriedTheNode,
        "the skinned mesh node's own 100 m translation reaches no vertex, which is what glTF states "
        "MUST happen and what a flatten applying it as well would fail silently");

  /* AND THE ORDER OF THE PRODUCT, likewise its own check: `R * inverseBind` sends (1, 0, 0) to
   * (0, 3, 0) and the swapped product sends it to (2, 1, 0), so one number separates them. */
  CHECK_NEAR(at[3], 0.0, 1e-9, "m", "vertex 1's x is the rotation applied AFTER the inverse bind");
  CHECK_NEAR(at[4], 3.0, 1e-9, "m", "vertex 1's y is x + 2 turned onto +y, not the swapped product's 1");

  Note("vertices skinned", 3.0, "over 2 joints");
  Covers("I.26.6 skinning: linear blend over joint matrices, and the skinned node's transform ignored");
  return Report();
}

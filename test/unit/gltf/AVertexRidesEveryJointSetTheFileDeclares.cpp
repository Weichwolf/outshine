#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"
#include "Subject.h"

using outshine::Span;
using outshine::Gltf::Document;
using outshine::Gltf::Subject;
using outshine::Test::Append;
using outshine::Test::Glb;

namespace {

constexpr size_t kJoints = 8;
constexpr double kStep = 1.0;

std::vector<uint8_t> Binary() {
  std::vector<uint8_t> out;
  for (int at = 0; at < 3; ++at) {
    Append(out, 0.0f);
    Append(out, 0.0f);
    Append(out, 0.0f);
  }
  for (int at = 0; at < 3; ++at) {
    for (uint16_t joint = 0; joint < 4; ++joint) { Append(out, joint); }
  }
  for (int at = 0; at < 3; ++at) {
    for (uint16_t joint = 4; joint < 8; ++joint) { Append(out, joint); }
  }
  for (int at = 0; at < 3; ++at) {
    for (int slot = 0; slot < 4; ++slot) { Append(out, 0.125f); }
  }
  for (int at = 0; at < 3; ++at) {
    for (int slot = 0; slot < 4; ++slot) { Append(out, 0.125f); }
  }
  for (size_t joint = 0; joint < kJoints; ++joint) {
    const float column[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (const float value : column) { Append(out, value); }
  }
  for (const uint16_t index : {uint16_t{0}, uint16_t{1}, uint16_t{2}}) { Append(out, index); }
  return out;
}

std::string Json(bool secondSet) {
  std::string nodes = R"({ "name": "rig", "children": [1, 2, 3, 4, 5, 6, 7, 8, 9] })";
  for (size_t joint = 0; joint < kJoints; ++joint) {
    nodes += ",\n    { \"name\": \"joint" + std::to_string(joint) + "\", \"translation\": [" +
             std::to_string((double)joint * kStep) + ", 0, 0] }";
  }
  nodes += ",\n    { \"name\": \"skinned\", \"mesh\": 0, \"skin\": 0 }";

  const std::string attributes =
      secondSet ? R"("POSITION": 0, "JOINTS_0": 1, "JOINTS_1": 2, "WEIGHTS_0": 3, "WEIGHTS_1": 4)"
                : R"("POSITION": 0, "JOINTS_0": 1, "WEIGHTS_0": 3)";

  return std::string(R"({
  "asset": { "version": "2.0" },
  "scenes": [ { "nodes": [0] } ],
  "nodes": [
    )") + nodes + R"(
  ],
  "meshes": [ { "primitives": [ { "attributes": { )" + attributes + R"( }, "indices": 6 } ] } ],
  "buffers": [ { "byteLength": 698 } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,   "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36,  "byteLength": 24 },
    { "buffer": 0, "byteOffset": 60,  "byteLength": 24 },
    { "buffer": 0, "byteOffset": 84,  "byteLength": 48 },
    { "buffer": 0, "byteOffset": 132, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 180, "byteLength": 512 },
    { "buffer": 0, "byteOffset": 692, "byteLength": 6 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
      "min": [0, 0, 0], "max": [1, 1, 0] },
    { "bufferView": 1, "componentType": 5123, "count": 3, "type": "VEC4" },
    { "bufferView": 2, "componentType": 5123, "count": 3, "type": "VEC4" },
    { "bufferView": 3, "componentType": 5126, "count": 3, "type": "VEC4" },
    { "bufferView": 4, "componentType": 5126, "count": 3, "type": "VEC4" },
    { "bufferView": 5, "componentType": 5126, "count": 8, "type": "MAT4" },
    { "bufferView": 6, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ],
  "skins": [ { "joints": [1, 2, 3, 4, 5, 6, 7, 8], "inverseBindMatrices": 5 } ]
})";
}

bool StandsAt(bool secondSet, double &x, std::string &why) {
  const std::vector<uint8_t> glb = Glb(Json(secondSet), Binary());
  Document document;
  if (!document.Read({glb.data(), glb.size()}, "influences.glb")) {
    why = document.Error();
    return false;
  }
  Subject subject;
  if (!subject.Build(document)) {
    why = subject.Error();
    return false;
  }
  if (subject.PositionsM().size() < 3) {
    why = "the subject carries no vertex";
    return false;
  }
  x = subject.PositionsM()[0];
  return true;
}

}

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> binary = Binary();
  CHECK(binary.size() == 698, "the fixture's binary chunk is the length its views address");

  double both = 0.0, one = 0.0;
  std::string why;
  const bool read = StandsAt(true, both, why);
  if (!read) { std::printf("REFUSED %s\n", why.c_str()); }
  CHECK(read, "a primitive that declares two joint sets is read");
  if (!read) { return Report(); }

  const double eightJoints = (0.0 + 1.0 + 2.0 + 3.0 + 4.0 + 5.0 + 6.0 + 7.0) / 8.0 * kStep;
  Note("the vertex stands at", both, "m");
  Note("eight joints at an eighth each put it at", eightJoints, "m");
  CHECK_NEAR(both, eightJoints, 1e-9, "m",
             "a vertex rides EVERY joint set the file declares, so eight influences at an eighth "
             "each land on the mean of eight joints and not on the mean of four");

  const bool alone = StandsAt(false, one, why);
  CHECK(alone, "the same fixture with only its first set is read too");
  const double fourJoints = (0.0 + 1.0 + 2.0 + 3.0) / 4.0 * kStep;
  Note("the same vertex with JOINTS_0 alone", one, "m");
  CHECK_NEAR(one, fourJoints, 1e-9, "m",
             "and dropping the second set moves it, which is what says the second set was carried "
             "rather than agreeing with the first by construction");
  CHECK(std::fabs(both - one) > 1.0,
        "the two answers are 2 m apart, so this is a discriminator and not a tolerance");

  Covers("I.77 skinning: a vertex is placed by the weighted blend of every JOINTS_n set the "
         "primitive declares, which glTF permits without bound and no case of the corpus at the pin "
         "carries -- 0 of 34 of the generator's animation models declare more than four influences");
  return Report();
}

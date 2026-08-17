/* THE LINK BETWEEN A TIME AND A DRAWABLE, HELD IN ARITHMETIC (board:1169). The sampler was already
 * right about its own numbers -- `ASamplerIsWhatTheFileSaysBetweenItsKeyframes` sweeps all three
 * interpolations over all three paths -- and nothing carried its answer into the hierarchy. What is
 * new here is the composition: a parent that translates and a child that rotates, and a vertex that
 * has to arrive where T(parent) * R(child) puts it.
 *
 * THE TIME IS A QUARTER OF THE ROTATION SPAN AND THAT IS THE WHOLE POINT. At the midpoint the
 * normalised component blend and the spherical one are both the bisector and agree exactly, so a
 * pose checked only at keyframes and midpoints passes over a component lerp. At a quarter of a
 * 90-degree span they are 22.5 and 21.598 degrees apart -- the number the sampler test already
 * measured -- and the vertex below is placed by the first of the two.
 *
 * AND THE POSE COVERS EVERY NODE, DRIVEN OR NOT. The triangle's own node carries the rotation and
 * its parent carries the translation; a run that held only the driven ones would place the child
 * correctly and lose the parent, which is the failure the length refusal makes unspellable. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Glb.h"

#include "Document.h"
#include "Pose.h"
#include "Subject.h"

using outshine::Span;
using outshine::Gltf::Document;
using outshine::Gltf::Pose;
using outshine::Gltf::Subject;
using outshine::Gltf::Transform;
using outshine::Test::Append;

namespace {

/* THE ROTATION SPAN'S SECOND KEYFRAME: a quarter turn about +Z, xyzw. Written as the arithmetic so
 * the angle is what is stated and the components follow from it. */
const double kQuarterTurn = std::sqrt(0.5);

constexpr size_t kBufferBytes = 116;

std::vector<uint8_t> Buffer() {
  std::vector<uint8_t> bytes;
  /* POSITION, three vertices. */
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  for (const float value : positions) { Append(bytes, value); }
  /* The triangle. */
  for (const uint16_t index : {uint16_t{0}, uint16_t{1}, uint16_t{2}}) { Append(bytes, index); }
  outshine::Test::PadTo4(bytes, 0xAB);
  /* The rotation sampler: two keyframes, a second apart, identity to a quarter turn about +Z. */
  Append(bytes, 0.0f);
  Append(bytes, 1.0f);
  for (const float value : {0.0f, 0.0f, 0.0f, 1.0f}) { Append(bytes, value); }
  for (const float value : {0.0f, 0.0f, (float)kQuarterTurn, (float)kQuarterTurn}) {
    Append(bytes, value);
  }
  /* The translation sampler: the same two instants, the origin to four metres along +X. */
  Append(bytes, 0.0f);
  Append(bytes, 1.0f);
  for (const float value : {0.0f, 0.0f, 0.0f, 4.0f, 0.0f, 0.0f}) { Append(bytes, value); }
  return bytes;
}

const char *kJson = R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"name": "carrier", "children": [1], "translation": [0, 0, 0]},
    {"name": "spinner", "mesh": 0, "rotation": [0, 0, 0, 1]}
  ],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
  "animations": [{
    "channels": [
      {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
      {"sampler": 1, "target": {"node": 0, "path": "translation"}}
    ],
    "samplers": [
      {"input": 2, "interpolation": "LINEAR", "output": 3},
      {"input": 4, "interpolation": "LINEAR", "output": 5}
    ]
  }],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
    {"bufferView": 2, "componentType": 5126, "count": 2, "type": "SCALAR",
     "min": [0], "max": [1]},
    {"bufferView": 3, "componentType": 5126, "count": 2, "type": "VEC4"},
    {"bufferView": 4, "componentType": 5126, "count": 2, "type": "SCALAR",
     "min": [0], "max": [1]},
    {"bufferView": 5, "componentType": 5126, "count": 2, "type": "VEC3"}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 6},
    {"buffer": 0, "byteOffset": 44, "byteLength": 8},
    {"buffer": 0, "byteOffset": 52, "byteLength": 32},
    {"buffer": 0, "byteOffset": 84, "byteLength": 8},
    {"buffer": 0, "byteOffset": 92, "byteLength": 24}
  ],
  "buffers": [{"byteLength": 116}]
})";

/* THE SAME FILE WITH THE DRIVEN NODE SPELLING ITS PLACEMENT AS A MATRIX, which the format forbids
 * for exactly the reason the pose refuses it: there is no rotation component in a matrix for a
 * channel to write. */
const char *kMatrixJson = R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"name": "carrier", "children": [1], "translation": [0, 0, 0]},
    {"name": "spinner", "mesh": 0,
     "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]}
  ],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
  "animations": [{
    "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}],
    "samplers": [{"input": 2, "interpolation": "LINEAR", "output": 3}]
  }],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
    {"bufferView": 2, "componentType": 5126, "count": 2, "type": "SCALAR",
     "min": [0], "max": [1]},
    {"bufferView": 3, "componentType": 5126, "count": 2, "type": "VEC4"}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 6},
    {"buffer": 0, "byteOffset": 44, "byteLength": 8},
    {"buffer": 0, "byteOffset": 52, "byteLength": 32}
  ],
  "buffers": [{"byteLength": 116}]
})";

double Distance(const std::vector<double> &positions, size_t vertex, const double point[3]) {
  double square = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double off = positions[vertex * 3 + axis] - point[axis];
    square += off * off;
  }
  return std::sqrt(square);
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> buffer = Buffer();
  CHECK(buffer.size() == kBufferBytes, "the fixture's buffer is the length its declaration states");
  const std::vector<uint8_t> glb = outshine::Test::Glb(kJson, buffer);

  Document file;
  CHECK(file.Read(Span<const uint8_t>(glb.data(), glb.size()), "posed.glb"),
        "the fixture reads as a document");
  CHECK(file.Animations().size() == 1, "the fixture carries the one animation it declares");

  std::string why;
  Pose pose;
  CHECK(Pose::Build(file, 0, pose, why), "the animation resolves into a pose");
  if (!why.empty()) { std::printf("NOTE refusal: %s\n", why.c_str()); }
  CHECK(pose.ChannelCount() == 2, "both channels of the animation are carried");
  CHECK(pose.NodeCount() == file.Nodes().size(),
        "the pose covers every node of the file and not only the driven ones");
  CHECK_NEAR(pose.StartS(), 0.0, 0.0, "s", "the pose's grid starts where the file's keyframes do");
  CHECK_NEAR(pose.EndS(), 1.0, 0.0, "s", "the pose's grid ends where the file's keyframes do");

  /* A QUARTER OF THE SPAN: the one place the two candidate rotation blends disagree by a measurable
   * angle, and the closed form below is the spherical one glTF states. */
  const double when = 0.25;
  std::vector<Transform> locals;
  pose.At(when, locals);
  CHECK(locals.size() == file.Nodes().size(), "a pose writes one local transform per node");

  Subject posed;
  CHECK(posed.Build(file, Span<const Transform>(locals.data(), locals.size())),
        "the document flattens at the pose");
  if (!posed.Error().empty()) { std::printf("NOTE %s\n", posed.Error().c_str()); }
  CHECK(posed.VertexCount() == 3, "the posed flatten carries the fixture's three vertices");

  const double angle = 0.25 * (M_PI / 2);
  const double carried = 4.0 * when;
  const double expected[3] = {carried + std::cos(angle), std::sin(angle), 0.0};
  CHECK_NEAR(Distance(posed.PositionsM(), 1, expected), 0.0, 1e-7, "m",
             "the vertex arrives where the parent's translation and the child's spherical rotation "
             "put it");
  /* WHAT THE COMPONENT BLEND WOULD HAVE PUT THERE, measured rather than asserted: the two answers
   * have to be further apart than the tolerance above, or the claim is not about the blend. */
  const double lerpAngle = 2.0 * std::atan2(when * kQuarterTurn,
                                            (1.0 - when) + when * kQuarterTurn);
  const double lerped[3] = {carried + std::cos(lerpAngle), std::sin(lerpAngle), 0.0};
  const double apart = Distance(posed.PositionsM(), 1, lerped);
  Note("spherical angle", angle * 180.0 / M_PI, "deg");
  Note("component-blend angle", lerpAngle * 180.0 / M_PI, "deg");
  Note("distance from the vertex the component blend would place", apart, "m");
  CHECK(apart > 1e-3, "the two blends are far enough apart here that the claim is about the blend");

  /* THE MOTION ITSELF, which is what an animated render case must be able to state before it
   * compares anything (board:1169): the drawn vertex at this time is not the drawn vertex at the
   * start of the grid. */
  std::vector<Transform> atStart;
  pose.At(0.0, atStart);
  Subject rest;
  CHECK(rest.Build(file, Span<const Transform>(atStart.data(), atStart.size())),
        "the document flattens at the start of the grid too");
  double moved = 0;
  for (size_t vertex = 0; vertex < rest.VertexCount(); ++vertex) {
    const double point[3] = {rest.PositionsM()[vertex * 3], rest.PositionsM()[vertex * 3 + 1],
                             rest.PositionsM()[vertex * 3 + 2]};
    const double off = Distance(posed.PositionsM(), vertex, point);
    moved = off > moved ? off : moved;
  }
  Note("furthest a vertex moved between the start of the grid and a quarter of the span", moved,
       "m");
  CHECK(moved > 1e-3, "the subject moves, so a comparison across the grid is comparing two poses");

  /* THE POSE AT THE START OF THE GRID IS THE FILE'S OWN REST POSE, which is what makes a still case
   * and the first frame of an animated one the same picture. */
  Subject unposed;
  CHECK(unposed.Build(file), "the document flattens with no pose at all");
  double drift = 0;
  for (size_t vertex = 0; vertex < unposed.VertexCount(); ++vertex) {
    const double point[3] = {unposed.PositionsM()[vertex * 3], unposed.PositionsM()[vertex * 3 + 1],
                             unposed.PositionsM()[vertex * 3 + 2]};
    const double off = Distance(rest.PositionsM(), vertex, point);
    drift = off > drift ? off : drift;
  }
  CHECK_NEAR(drift, 0.0, 0.0, "m",
             "the pose at the first keyframe is the file's own placement, bit for bit");

  Subject wrongLength;
  CHECK(!wrongLength.Build(file, Span<const Transform>(locals.data(), locals.size() - 1)),
        "a pose one node short is refused rather than applied to the nodes it does cover");
  std::printf("NOTE %s\n", wrongLength.Error().c_str());

  const std::vector<uint8_t> matrixGlb = outshine::Test::Glb(kMatrixJson, buffer);
  Document matrixFile;
  CHECK(matrixFile.Read(Span<const uint8_t>(matrixGlb.data(), matrixGlb.size()), "matrix.glb"),
        "the matrix fixture reads as a document");
  Pose refused;
  CHECK(!Pose::Build(matrixFile, 0, refused, why),
        "a driven node that spells its placement as a matrix is refused by name");
  std::printf("NOTE %s\n", why.c_str());

  Pose missing;
  CHECK(!Pose::Build(file, 1, missing, why),
        "an animation index the file does not carry is refused rather than defaulted to the first");
  std::printf("NOTE %s\n", why.c_str());

  /* A POSE IS BUILT FROM A DECLARED SET, AND THE SET IS NOT AN INDEX (board:1198). A file's animations
   * are independent and a client plays any subset, so the subset is the caller's declaration -- and the
   * three statements below are the three ways that declaration can be wrong. */
  const int justTheFirst[1] = {0};
  Pose asASet;
  CHECK(Pose::Build(file, Span<const int>(justTheFirst, 1), asASet, why),
        "a set naming one animation resolves");
  CHECK(asASet.ChannelCount() == pose.ChannelCount() && asASet.NodeCount() == pose.NodeCount() &&
            asASet.StartS() == pose.StartS() && asASet.EndS() == pose.EndS(),
        "the set of one and the single index build the same pose, so the overload is one spelling of "
        "one operation and not two operations");

  Pose empty;
  CHECK(!Pose::Build(file, Span<const int>(justTheFirst, 0), empty, why),
        "an empty set is refused, and that is a different statement from a file carrying none");
  std::printf("NOTE %s\n", why.c_str());

  /* THE SAME ANIMATION TWICE IS THE CONFLICT, EXACTLY: both entries claim the same node's same path,
   * which is what two animations driving one property would do and what the format leaves undefined. */
  const int twice[2] = {0, 0};
  Pose contested;
  CHECK(!Pose::Build(file, Span<const int>(twice, 2), contested, why),
        "two animations driving one node's same path are refused naming both, rather than the last "
        "one silently winning");
  std::printf("NOTE %s\n", why.c_str());

  Covers("I.26 animations: samplers, channels and the node hierarchy they drive");
  return Report();
}

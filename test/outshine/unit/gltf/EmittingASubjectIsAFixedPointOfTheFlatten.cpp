/* `Subject(Emit(S)) == S`, EXACT -- positions, indices, part boundaries, part materials, tangent
 * provenance, and the two attribute runs a part may or may not carry.
 *
 * IT IS A FIXED POINT OF THE FLATTEN AND NOT OF THE DOCUMENT, and the difference is the whole test.
 * `Document -> Document` is NOT identity: the flatten discards the hierarchy, so a hierarchy of
 * nested nodes and a flat list of parts are two documents with ONE subject, and a round-trip
 * asserted on the document would be asserting something false. The fixture below carries a nested,
 * transformed hierarchy for exactly that reason -- the emitted file is a different document and must
 * be the same subject.
 *
 * AND WHERE THE POINT IS REACHED DEPENDS ON THE SUBJECT, SO BOTH ARMS ARE MEASURED HERE. `Subject`
 * holds doubles and glTF's POSITION is f32. A subject whose nodes are at the identity carries the
 * file's own f32 values, so `Subject(Emit(S)) == S` holds at ZERO applications and that is the
 * section's acceptance, asserted on the first fixture. A subject whose nodes ROTATED it carries
 * doubles no f32 represents, so the first emit narrows them and every emit after that changes
 * nothing: the point is reached after one application, the narrowing is MEASURED against one f32
 * ulp rather than hidden, and `Emit(S)` and `Emit(Subject(Emit(S)))` are asserted BYTE-IDENTICAL --
 * which is what would catch an emitter whose output depended on how many times it had run. */
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"
#include "Emit.h"
#include "Subject.h"

using outshine::Gltf::Document;
using outshine::Gltf::Emission;
using outshine::Gltf::MaterialRef;
using outshine::Gltf::Part;
using outshine::Gltf::Subject;
using outshine::Gltf::TangentSource;
using outshine::Test::Append;
using outshine::Test::Glb;

namespace {

/* Two triangles that share nothing: one carries NORMAL and TEXCOORD_0 and a supplied TANGENT, the
 * other carries POSITION alone -- so the emitted file has to state a different attribute set per
 * primitive, and `HasUv`/`HasNormal`/`Tangent` have something to be wrong about. */
constexpr float kPosition[6][3] = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f},
                                   {2.f, 0.f, 0.f}, {3.f, 0.f, 0.f}, {2.f, 1.f, 0.f}};
constexpr float kNormal[3][3] = {{0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}};
constexpr float kUv[3][2] = {{0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}};
constexpr float kTangent[3][4] = {{1.f, 0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}};
constexpr uint16_t kIndex[3] = {0, 1, 2};
/* AND A `COLOR_0` ON THE FIRST PRIMITIVE ONLY (board:1193), so the emitted file states the semantic
 * on one primitive and not the other. The values are dyadic and inside [0, 1], which is what a
 * fixed point over an f32 attribute needs: a colour the narrowing moved would be measuring the
 * conversion instead of the round trip, and one outside the range is a file the reader refuses. */
constexpr float kColour[3][4] = {{0.f, 0.5f, 1.f, 1.f},
                                 {0.25f, 1.f, 0.f, 0.5f},
                                 {1.f, 0.f, 0.75f, 0.25f}};

std::vector<uint8_t> Binary() {
  std::vector<uint8_t> bytes;
  for (const auto &vertex : kPosition) {
    for (const float axis : vertex) { Append(bytes, axis); }
  }
  for (const auto &normal : kNormal) {
    for (const float axis : normal) { Append(bytes, axis); }
  }
  for (const auto &uv : kUv) {
    for (const float axis : uv) { Append(bytes, axis); }
  }
  for (const auto &tangent : kTangent) {
    for (const float axis : tangent) { Append(bytes, axis); }
  }
  for (const uint16_t index : kIndex) { Append(bytes, index); }
  /* Two bytes of padding, so the colour accessor starts on a multiple of its own component size --
   * the six unsigned-short indices before it leave the run at 186. */
  Append(bytes, uint16_t{0});
  for (const auto &colour : kColour) {
    for (const float channel : colour) { Append(bytes, channel); }
  }
  return bytes;
}

/* A NESTED, ROTATED, TRANSLATED HIERARCHY over two meshes. The turn is 45 degrees about Y and NOT a
 * right angle: a quarter turn permutes axis-aligned coordinates and would leave every vertex on an
 * f32 it already was, so the arm that is supposed to measure the narrowing would have measured
 * nothing. Measured, and it is why this fixture says 45: at 90 degrees the worst movement was
 * 7.45e-09 f32 ulps, which is a double residual and not a conversion. */
enum class Placed { AtTheIdentity, TurnedAndScaled };

std::string Fixture(Placed how) {
  const char *nodes =
      how == Placed::AtTheIdentity
          ? R"( { "name": "root", "children": [1, 2] },
    { "name": "turned", "mesh": 0 },
    { "name": "plain", "mesh": 1 } )"
          : R"( { "name": "root", "translation": [0.25, -0.5, 1.5], "children": [1, 2] },
    { "name": "turned", "mesh": 0,
      "rotation": [0.0, 0.3826834323650898, 0.0, 0.9238795325112867] },
    { "name": "plain", "mesh": 1, "scale": [1.5, 1.0, 2.0] } )";
  return std::string(R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [)") + nodes + R"(],
  "buffers": [ { "byteLength": 236 } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,   "byteLength": 72 },
    { "buffer": 0, "byteOffset": 72,  "byteLength": 36 },
    { "buffer": 0, "byteOffset": 108, "byteLength": 24 },
    { "buffer": 0, "byteOffset": 132, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 180, "byteLength": 6 },
    { "buffer": 0, "byteOffset": 188, "byteLength": 48 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
      "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0], "byteOffset": 0 },
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
      "min": [2.0, 0.0, 0.0], "max": [3.0, 1.0, 0.0], "byteOffset": 36 },
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2" },
    { "bufferView": 3, "componentType": 5126, "count": 3, "type": "VEC4" },
    { "bufferView": 4, "componentType": 5123, "count": 3, "type": "SCALAR" },
    { "bufferView": 5, "componentType": 5126, "count": 3, "type": "VEC4" }
  ],
  "materials": [
    { "name": "matte", "pbrMetallicRoughness": {
        "baseColorFactor": [0.25, 0.5, 0.75, 0.5], "metallicFactor": 0.125,
        "roughnessFactor": 0.375 },
      "emissiveFactor": [0.0625, 0.125, 0.25], "alphaMode": "MASK", "alphaCutoff": 0.25,
      "doubleSided": true },
    { "name": "flat", "pbrMetallicRoughness": { "baseColorFactor": [1.0, 1.0, 1.0, 1.0] },
      "extensions": { "KHR_materials_unlit": {} } }
  ],
  "meshes": [
    { "primitives": [ { "attributes": { "POSITION": 0, "NORMAL": 2, "TEXCOORD_0": 3,
                                        "TANGENT": 4, "COLOR_0": 6 },
                        "indices": 5, "material": 0 } ] },
    { "primitives": [ { "attributes": { "POSITION": 1 }, "indices": 5, "material": 1 } ] }
  ]
})";
}

/* The one place the two subjects are held against each other, so "the same" has one spelling. Every
 * comparison is exact: a tolerance here would be a tolerance on a serialiser. */
[[nodiscard]] bool Same(const Subject &left, const Subject &right, std::string &differs) {
  const auto say = [&differs](const char *what) {
    differs = what;
    return false;
  };
  if (left.PositionsM() != right.PositionsM()) { return say("the positions"); }
  if (left.Normals() != right.Normals()) { return say("the normals"); }
  if (left.Uv() != right.Uv()) { return say("the uvs"); }
  if (left.Tangents() != right.Tangents()) { return say("the tangents"); }
  if (left.Colours() != right.Colours()) { return say("the vertex colours"); }
  if (left.Indices() != right.Indices()) { return say("the indices"); }
  if (left.Parts().size() != right.Parts().size()) { return say("the part count"); }
  for (size_t at = 0; at < left.Parts().size(); ++at) {
    const Part &was = left.Parts()[at];
    const Part &is = right.Parts()[at];
    if (was.NodeName != is.NodeName) { return say("a part's node name"); }
    if (was.Material != is.Material) { return say("a part's material"); }
    if (was.HasUv != is.HasUv) { return say("a part's uv declaration"); }
    if (was.HasNormal != is.HasNormal) { return say("a part's normal declaration"); }
    if (was.HasColour != is.HasColour) { return say("a part's vertex colour declaration"); }
    if (was.Tangent != is.Tangent) { return say("a part's tangent provenance"); }
    if (was.FirstVertex != is.FirstVertex || was.VertexCount != is.VertexCount) {
      return say("a part's vertex run");
    }
    if (was.FirstIndex != is.FirstIndex || was.IndexCount != is.IndexCount) {
      return say("a part's index run");
    }
  }
  return true;
}

[[nodiscard]] bool Flatten(const std::vector<uint8_t> &glb, Document &file, Subject &out,
                           std::string &why) {
  if (!file.Read({glb.data(), glb.size()}, "emitted.glb")) {
    why = file.Error();
    return false;
  }
  if (!out.Build(file)) {
    why = out.Error();
    return false;
  }
  return true;
}

Emission Over(const Subject &subject, const Document &file) {
  Emission what;
  what.Geometry = &subject;
  what.Materials = {file.Materials().data(), file.Materials().size()};
  what.Generator = "outshine test";
  return what;
}

/* THE LARGEST DISTANCE ANY COMPONENT MOVED, IN f32 ULPS AT THAT COMPONENT'S OWN MAGNITUDE. It is
 * reported rather than compared against a fixed epsilon, because the only quantity the narrowing can
 * cost is representation: a double rounded to the nearest f32 is within half an ulp of itself, and
 * anything above one ulp is arithmetic somebody did, not a conversion. */
double WorstUlps(const std::vector<double> &was, const std::vector<double> &is) {
  double worst = 0.0;
  for (size_t at = 0; at < was.size() && at < is.size(); ++at) {
    const float value = static_cast<float>(was[at]);
    const float next = std::nextafter(value, std::numeric_limits<float>::infinity());
    const double ulp = (double)next - (double)value;
    const double moved = std::fabs(was[at] - is[at]);
    if (ulp > 0.0 && moved / ulp > worst) { worst = moved / ulp; }
  }
  return worst;
}

/* One arm of the claim: read a fixture, emit it, read it back. Everything below is stated about the
 * three subjects this produces. */
struct RoundTrip {
  Document Read, Again;
  Subject First, Second;
  std::vector<uint8_t> Source, Emitted;
  bool Ok = false;
  std::string Why;
};

RoundTrip Through(Placed how, const std::vector<uint8_t> &binary) {
  RoundTrip trip;
  trip.Source = Glb(Fixture(how), binary);
  if (!Flatten(trip.Source, trip.Read, trip.First, trip.Why)) { return trip; }
  if (!outshine::Gltf::Emit(Over(trip.First, trip.Read), trip.Emitted, trip.Why)) { return trip; }
  if (!Flatten(trip.Emitted, trip.Again, trip.Second, trip.Why)) { return trip; }
  trip.Ok = true;
  return trip;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> binary = Binary();
  CHECK(binary.size() == 236, "the fixture's binary chunk is the length its buffer declares");

  /* THE SECTION'S OWN ACCEPTANCE, ON THE ARM WHERE IT IS EXACT AT ZERO APPLICATIONS: nodes at the
   * identity, so every number the flatten produced is the f32 the file carried. */
  RoundTrip still = Through(Placed::AtTheIdentity, binary);
  CHECK(still.Ok, "a subject at the identity flattens, emits and reads back");
  if (!still.Ok) {
    std::printf("       %s\n", still.Why.c_str());
    return Report();
  }
  CHECK(still.First.Parts().size() == 2, "the hierarchy flattens into two parts");
  CHECK(still.First.HasNormal() && still.First.HasUv() && still.First.HasTangent(),
        "one part carries every attribute the writer has to state per primitive");
  if (still.First.Parts().size() == 2) {
    CHECK(still.First.Parts()[0].Tangent == TangentSource::Supplied,
          "the supplied tangent basis is the one the file wrote, so its PROVENANCE is what the "
          "round trip has to carry");
    /* **THE DISCRIMINATOR IS THE UV SET AND NO LONGER THE NORMAL** (board:1471). This part was
     * POSITION alone and the claim was that a writer stating one attribute set for the whole subject
     * would be caught by it. glTF requires a reader to CALCULATE flat normals where a primitive
     * declares none -- *client implementations MUST calculate flat normals* -- so a part read out of a
     * file never carries POSITION alone again, and a test asserting it would be asserting against the
     * format. The uv set is untouched by that rule and catches the same writer. */
    CHECK(still.First.Parts()[1].HasNormal && !still.First.Parts()[1].HasUv,
          "the second part carries no uv set while the first does, so a writer that stated one "
          "attribute set for the whole subject would be caught here -- and it carries the flat "
          "normal the format requires a reader to calculate");
  }
  std::string differs;
  const bool exact = Same(still.First, still.Second, differs);
  CHECK(exact, "Subject(Emit(S)) == S, exactly, for a subject whose numbers the format can hold");
  if (!exact) { std::printf("       %s differ\n", differs.c_str()); }

  /* THE DOCUMENTS ARE NOT THE SAME AND THAT IS THE POINT. Three nodes became two at the identity;
   * asserting the document would assert something false. */
  CHECK(still.Again.Nodes().size() != still.Read.Nodes().size(),
        "the emitted DOCUMENT differs from the source document, because the flatten discarded the "
        "hierarchy and nothing here invents one");

  /* THE OTHER ARM: a rotation no f32 holds, so the first emit narrows and the claim is about where
   * the point is rather than whether there is one. */
  RoundTrip turned = Through(Placed::TurnedAndScaled, binary);
  CHECK(turned.Ok, "a rotated, scaled, translated subject flattens, emits and reads back");
  if (!turned.Ok) {
    std::printf("       %s\n", turned.Why.c_str());
    return Report();
  }
  const bool narrowed = turned.First.PositionsM() != turned.Second.PositionsM();
  CHECK(narrowed, "the rotated subject's positions DO move, so this arm measures the narrowing "
                  "rather than a case that happens not to need it");
  Note("worst position movement across the first emit",
       WorstUlps(turned.First.PositionsM(), turned.Second.PositionsM()), "f32 ulps");
  CHECK(WorstUlps(turned.First.PositionsM(), turned.Second.PositionsM()) <= 1.0,
        "and it moves by at most one f32 ulp, which is what a conversion costs -- anything more "
        "would be arithmetic this writer is not entitled to do");

  std::vector<uint8_t> twice;
  std::string refusal;
  const bool wroteAgain =
      outshine::Gltf::Emit(Over(turned.Second, turned.Again), twice, refusal);
  CHECK(wroteAgain, "the re-read subject emits again");
  CHECK(wroteAgain && twice == turned.Emitted,
        "Emit(Subject(Emit(S))) is byte-identical to Emit(S), so ONE application reaches the fixed "
        "point and no later one moves it");

  Document third;
  Subject settled;
  std::string why;
  const bool reread = Flatten(twice, third, settled, why);
  CHECK(reread, "and the third read succeeds");
  const bool settledSame = reread && Same(turned.Second, settled, differs);
  CHECK(settledSame, "Subject(Emit(S')) == S' for S' = Subject(Emit(S)), which is the fixed point "
                     "stated on the subject rather than on the bytes");
  if (reread && !settledSame) { std::printf("       %s differ\n", differs.c_str()); }

  /* THE MATERIAL ROW IS THE SUBJECT'S OTHER HALF and it travels beside the geometry rather than
   * inside it: a part's material index means nothing without the table it indexes. */
  CHECK(still.Again.Materials().size() == still.Read.Materials().size(),
        "the material table crosses whole, so a part's material INDEX still names the same surface");
  bool rows = still.Again.Materials().size() == still.Read.Materials().size();
  for (size_t at = 0; rows && at < still.Read.Materials().size(); ++at) {
    const MaterialRef &was = still.Read.Materials()[at];
    const MaterialRef &is = still.Again.Materials()[at];
    rows = was.Name == is.Name && was.Surface.Metalness == is.Surface.Metalness &&
           was.Surface.Roughness == is.Surface.Roughness &&
           was.Surface.Alpha == is.Surface.Alpha &&
           was.Surface.CoverageCut == is.Surface.CoverageCut &&
           was.Surface.DoubleSided == is.Surface.DoubleSided &&
           was.Surface.Unlit == is.Surface.Unlit;
    for (int channel = 0; channel < 4; ++channel) {
      rows = rows && was.Surface.BaseColour[channel] == is.Surface.BaseColour[channel];
    }
    for (int channel = 0; channel < 3; ++channel) {
      rows = rows && was.Surface.Emission[channel] == is.Surface.Emission[channel];
    }
  }
  CHECK(rows, "every material crosses exactly -- name, base colour, metalness, roughness, emission, "
              "alpha mode, cutoff, two-sidedness and whether it reads light at all");

  /* WHAT THE WRITER CANNOT STATE IS A REFUSAL THAT NAMES IT, never a file with the part left out. */
  Subject empty;
  Emission nothing;
  nothing.Geometry = &empty;
  std::vector<uint8_t> unwritten;
  CHECK(!outshine::Gltf::Emit(nothing, unwritten, refusal),
        "a subject that draws no part is refused rather than written as an empty file");
  CHECK(refusal.find("no part") != std::string::npos, "and the refusal says which");

  Covers("I.28 the emit path: Subject -> Document -> bytes, a serialiser and not a translation, "
         "whose acceptance is Subject(Emit(S)) == S over positions, indices, part boundaries, part "
         "materials and tangent provenance");
  return Report();
}

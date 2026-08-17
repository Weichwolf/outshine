/* THE SECOND UV SET IS READ AS ITS OWN STREAM, AND THE REFUSAL board:1177 STATED NARROWS RATHER THAN
 * DISAPPEARS (board:1182).
 *
 * TWO CLAIMS, AND THE SECOND IS THE DANGEROUS ONE.
 *
 * FIRST: a primitive declaring `TEXCOORD_1` yields a second run that is the file's own numbers and
 * not a copy of the first. `MultiUVTest` is the asset that shows it, and the trap it is built for is
 * an engine that reads set 0 twice: its two accessors sit in two buffer views and place one face a
 * quarter of the image apart, so the reading decides WHERE the emissive image lands and not whether
 * it lands. That is a silent success, not a disagreement -- an image on the wrong uv set is still an
 * image on a surface -- so the numbers are checked here rather than left to a picture alone.
 *
 * SECOND: a texture reference naming `TEXCOORD_1` on a subject that carries ONE uv set is a NAMED
 * REFUSAL, and it stays one now that the engine can bind two. The refusal was never about the
 * engine's capability; it is about the SUBJECT's attributes, and the fall-back it forbids -- read the
 * first set instead -- is exactly what `MultiUVTest` writes "Multiple UVs not supported in this
 * viewer" into the frame to catch. A round that turned this into a fall-back would pass every case
 * in the corpus and ship the defect green.
 *
 * AND THE THIRD ARM IS THE ENGINE'S OWN BOUND: `TEXCOORD_2` and beyond is a refusal that names how
 * many sets are bound, because a set no vertex layout carries is not a set that can be read.
 *
 * NOTHING HERE RENDERS. Which uv set a reference reads is a computation over the file's declaration
 * and the subject's attributes, so the instrument is a stated invariant and not a picture. */
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"
#include "Document.h"
#include "Span.h"
#include "Subject.h"
#include "Types.h"

namespace {

using outshine::Gltf::CarriedUvSets;
using outshine::Gltf::Document;
using outshine::Gltf::Subject;
using outshine::Gltf::TextureRef;
using outshine::Gltf::UvSetOf;
using outshine::UvSet;

/* One quad whose two uv sets address two different quarters of the image, with the emissive
 * reference reading the second and the base colour reading the first -- `MultiUVTest`'s shape,
 * spelled small. `secondSet` false leaves TEXCOORD_1 out entirely, which is the refusing subject.
 *
 * A GLB AND NOT A `data:` URI, because this reader decodes no data URI and a temporary file would
 * put a filesystem between the claim and the bytes it is about. */
std::string Text(bool secondSet) {
  const std::string attributes = secondSet ? "\"POSITION\":0,\"TEXCOORD_0\":1,\"TEXCOORD_1\":2"
                                           : "\"POSITION\":0,\"TEXCOORD_0\":1";
  return std::string(
             "{\"asset\":{\"version\":\"2.0\"},"
             "\"images\":[{\"uri\":\"one.png\"}],"
             "\"textures\":[{\"source\":0}],"
             "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}},"
             "\"emissiveTexture\":{\"index\":0,\"texCoord\":1}}],"
             "\"meshes\":[{\"primitives\":[{\"attributes\":{") +
         attributes +
         "},\"indices\":3,\"material\":0}]}],"
         "\"nodes\":[{\"mesh\":0}],\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
         /* Three positions, three first-set uvs at the image's left edge, the same three shifted
          * 0.75 along u for the second set, and one triangle. The shift is uniform so the claim
          * below is one number, and it is non-zero at EVERY vertex so no vertex can agree by
          * accident. */
         "\"accessors\":["
         "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
         "\"min\":[0,0,0],\"max\":[1,1,0]},"
         "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
         "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
         "{\"bufferView\":3,\"componentType\":5125,\"count\":3,\"type\":\"SCALAR\"}],"
         "\"bufferViews\":["
         "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
         "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":24},"
         "{\"buffer\":0,\"byteOffset\":60,\"byteLength\":24},"
         "{\"buffer\":0,\"byteOffset\":84,\"byteLength\":12}],"
         "\"buffers\":[{\"byteLength\":96}]}";
}

/* THE BYTES THE ACCESSORS ADDRESS, written as the numbers rather than as a blob: the two uv runs are
 * the whole point of this case and a base64 literal would hide which coordinates they are. */
std::vector<uint8_t> Binary() {
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const float first[6] = {0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.25f};
  const float second[6] = {0.75f, 0.0f, 1.0f, 0.0f, 0.75f, 0.25f};
  const uint32_t indices[3] = {0, 1, 2};
  std::vector<uint8_t> out;
  for (const float value : positions) { outshine::Test::Append(out, value); }
  for (const float value : first) { outshine::Test::Append(out, value); }
  for (const float value : second) { outshine::Test::Append(out, value); }
  for (const uint32_t value : indices) { outshine::Test::Append(out, value); }
  return out;
}

bool Reads(bool secondSet, Document &into) {
  const std::vector<uint8_t> glb = outshine::Test::Glb(Text(secondSet), Binary());
  return into.Read(outshine::Span<const uint8_t>(glb.data(), glb.size()), "");
}

constexpr double kTolerance = 5.9604644775390625e-08; /* half an f32 ulp at 1.0 */

} // namespace

int main() {
  using namespace outshine::Test;
  Covers("board:1182");

  Document both;
  CHECK(Reads(true, both), "a primitive declaring TEXCOORD_0 and TEXCOORD_1 is read");
  Subject carried;
  CHECK(carried.Build(both), "and it flattens into a subject");
  CHECK(carried.HasUv(), "which carries the first uv set");
  CHECK(carried.HasUv1(), "and the second one, as a run of its own");
  CHECK(carried.Parts().size() == 1 && carried.Parts()[0].HasUv1,
        "and the part says so, per primitive rather than per subject");
  CHECK(carried.Uv().size() == carried.Uv1().size(),
        "the two runs cover the same vertices, so a consumer binds them in step");

  /* THE NUMBERS, AND THIS IS THE CLAIM AN ENGINE READING SET 0 TWICE FAILS. Every vertex differs
   * between the two sets by 0.75 uv units, so an implementation that read the first accessor into
   * both runs agrees here at exactly zero vertices. */
  double closest = 1e9;
  for (size_t vertex = 0; vertex < carried.VertexCount(); ++vertex) {
    const double du = carried.Uv1()[vertex * 2] - carried.Uv()[vertex * 2];
    const double dv = carried.Uv1()[vertex * 2 + 1] - carried.Uv()[vertex * 2 + 1];
    const double apart = std::sqrt(du * du + dv * dv);
    if (apart < closest) { closest = apart; }
  }
  CHECK_NEAR(closest, 0.75, kTolerance, "uv",
             "the second set is the FILE's second accessor and not a second read of the first -- "
             "every vertex is 0.75 uv apart, and a copy would be 0");

  const auto &material = both.Materials()[0];
  CHECK(material.BaseColour.TexCoord == 0 && material.Emissive.TexCoord == 1,
        "the file states its two sockets on two different uv sets, which is what a per-reference "
        "field means");

  std::string why;
  UvSet set = UvSet::Second;
  CHECK(UvSetOf(material.BaseColour, CarriedUvSets::Both, "baseColorTexture", set, why),
        "a reference naming TEXCOORD_0 resolves");
  CHECK(set == UvSet::First, "to the first set");
  CHECK(UvSetOf(material.Emissive, CarriedUvSets::Both, "emissiveTexture", set, why),
        "a reference naming TEXCOORD_1 resolves on a subject that carries it -- the narrowing");
  CHECK(set == UvSet::Second, "to the SECOND set, which is what board:1182 adds");

  /* THE OTHER SIDE OF THE NARROWING, AND IT IS THE HALF THAT MUST NOT BECOME A FALL-BACK. */
  Document one;
  CHECK(Reads(false, one), "the same file without TEXCOORD_1 is read");
  Subject alone;
  CHECK(alone.Build(one), "and flattens");
  CHECK(!alone.HasUv1(), "carrying one uv set");
  CHECK(alone.Parts().size() == 1 && !alone.Parts()[0].HasUv1,
        "and no part of it claims a second");

  set = UvSet::Second;
  why.clear();
  const bool resolved =
      UvSetOf(one.Materials()[0].Emissive, CarriedUvSets::FirstOnly, "emissiveTexture", set, why);
  CHECK(!resolved,
        "a reference naming TEXCOORD_1 over a subject that carries one uv set is REFUSED and is "
        "never answered with the first set -- a fall-back here renders a plausible picture of the "
        "wrong thing, which no case can tell from an authored one");
  CHECK(why.find("TEXCOORD_1") != std::string::npos,
        "and the refusal names the set the file asked for");
  CHECK(why.find("first uv set only") != std::string::npos,
        "and what the subject carries instead, so the sentence says whose defect it is");

  /* AND THE ENGINE'S OWN BOUND, which is a different sentence from the one above: two sets are what
   * a vertex layout binds, so a third is not a subject's shortfall but this engine's. */
  TextureRef third;
  third.Texture = 0;
  third.TexCoord = 2;
  why.clear();
  CHECK(!UvSetOf(third, CarriedUvSets::Both, "baseColorTexture", set, why),
        "a reference naming TEXCOORD_2 is refused whatever the subject carries");
  CHECK(why.find("2 uv sets") != std::string::npos,
        "and the refusal names how many this engine binds, so the sentence is about the engine and "
        "not about the asset");

  return Report();
}

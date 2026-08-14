/* `KHR_texture_transform` IS PER TEXTURE REFERENCE AND NOT PER MATERIAL (board:1177), and this is the
 * claim no asset in the Khronos corpus states.
 *
 * MEASURED, at the pinned commit: `TextureTransformTest` gives each of its nine materials exactly one
 * `baseColorTexture` and no other socket, and `TextureTransformMultiTest` gives each of its
 * twenty-nine materials exactly one transformed reference -- base colour on one, emissive on the next,
 * normal on the next. So the corpus separates "transformed on every socket" from "transformed on the
 * base-colour socket only", and it never separates "one transform per reference" from "one transform
 * per material", because no file in it puts two DIFFERENT transforms on two references of one
 * material. A picture cannot decide what no picture spells, so it is decided here.
 *
 * FIVE SOCKETS, FIVE DIFFERENT TRANSFORMS, ONE MATERIAL. An engine carrying a single transform per
 * material returns one of the five for all of them, whichever it happened to keep, and every one of
 * the five claims below fails. An engine that reads the file's structure returns five.
 *
 * AND THE OVERRIDE IS THE OTHER HALF OF THE SAME SENTENCE. The extension's own `texCoord` REPLACES the
 * `textureInfo`'s when supplied and is silent when it is not, so both directions are stated: a
 * reference whose textureInfo names set 1 and whose extension names set 0 reads set 0, and one whose
 * extension names no set keeps what the textureInfo said. A reader that merged the two fields, or
 * preferred the wrong one, passes exactly half of that. */
#include <cmath>
#include <cstdint>
#include <string>

#include "Check.h"
#include "Document.h"
#include "Span.h"
#include "Types.h"

namespace {

using outshine::Gltf::Document;
using outshine::Gltf::MaterialRef;
using outshine::Gltf::TextureRef;

/* One document carrying one texture and whatever materials the caller spells, read from bytes: the
 * claim is about decoding and a temporary file would put a filesystem in the way of it. */
bool Reads(const std::string &materials, Document &into) {
  const std::string text =
      "{\"asset\":{\"version\":\"2.0\"},"
      "\"images\":[{\"uri\":\"one.png\"}],"
      "\"textures\":[{\"source\":0}],"
      "\"materials\":[" + materials + "]}";
  return into.Read(outshine::Span<const uint8_t>((const uint8_t *)text.data(), text.size()), "");
}

std::string Transformed(const char *offsetU, const char *rotation) {
  return std::string("{\"index\":0,\"extensions\":{\"KHR_texture_transform\":{\"offset\":[") +
         offsetU + ",0.0],\"rotation\":" + rotation + "}}}";
}

constexpr double kTolerance = 5.9604644775390625e-08; /* half an f32 ulp at 1.0 */

void CarriesOffsetAndTurn(const TextureRef &reference, double offsetU, double rotationRad,
                          const char *socket) {
  using namespace outshine::Test;
  CHECK_NEAR(reference.Transform.M[2], offsetU, kTolerance, "uv",
             (std::string("the ") + socket + " reference carries ITS OWN offset, not a neighbour's")
                 .c_str());
  CHECK_NEAR(reference.Transform.M[0], std::cos(rotationRad), kTolerance, "uv per uv",
             (std::string("and its own rotation: an engine keeping one transform per material would "
                          "answer another socket's turn on the ") + socket).c_str());
  /* THE SIGNED HALF OF THE TURN AND NOT ONLY ITS COSINE, which is even, so a flipped rotation would
   * otherwise pass every claim in this file (`core/UvTransform.h` holds WHICH sign is right). */
  CHECK_NEAR(reference.Transform.M[3], -std::sin(rotationRad), kTolerance, "uv per uv",
             (std::string("and carries the turn with its sign on the ") + socket).c_str());
}

} // namespace

int main() {
  using namespace outshine::Test;
  Covers("board:1177");

  /* ONE MATERIAL, FIVE SOCKETS, FIVE DISTINCT TRANSFORMS. */
  Document five;
  const bool read = Reads(
      "{\"pbrMetallicRoughness\":{\"baseColorTexture\":" + Transformed("0.1", "0.11") +
          ",\"metallicRoughnessTexture\":" + Transformed("0.2", "0.22") + "}" +
          ",\"normalTexture\":" + Transformed("0.3", "0.33") +
          ",\"occlusionTexture\":" + Transformed("0.4", "0.44") +
          ",\"emissiveTexture\":" + Transformed("0.5", "0.55") + "}",
      five);
  CHECK(read, "a material declaring KHR_texture_transform on five sockets reads");
  CHECK(five.Materials().size() == 1, "and yields one material");
  if (read && five.Materials().size() == 1) {
    const MaterialRef &material = five.Materials()[0];
    CarriesOffsetAndTurn(material.BaseColour, 0.1, 0.11, "baseColorTexture");
    CarriesOffsetAndTurn(material.MetallicRoughness, 0.2, 0.22, "metallicRoughnessTexture");
    CarriesOffsetAndTurn(material.Normal, 0.3, 0.33, "normalTexture");
    CarriesOffsetAndTurn(material.Occlusion, 0.4, 0.44, "occlusionTexture");
    CarriesOffsetAndTurn(material.Emissive, 0.5, 0.55, "emissiveTexture");
  }

  /* A REFERENCE THAT DECLARES NOTHING CARRIES THE IDENTITY, beside one that declares something -- so
   * the untransformed socket of a transformed material is not swept up by its neighbour either. */
  Document mixed;
  CHECK(Reads("{\"pbrMetallicRoughness\":{\"baseColorTexture\":" + Transformed("0.25", "0.5") +
                  "},\"emissiveTexture\":{\"index\":0}}",
              mixed),
        "a material with one transformed socket and one plain one reads");
  if (mixed.Materials().size() == 1) {
    const outshine::UvTransform identity;
    for (size_t element = 0; element < 6; ++element) {
      CHECK(mixed.Materials()[0].Emissive.Transform.M[element] == identity.M[element],
            "the socket that declares no extension carries the IDENTITY and not the transform of "
            "the socket beside it");
    }
  }

  /* THE `texCoord` OVERRIDE, BOTH DIRECTIONS. */
  Document overridden;
  CHECK(Reads("{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0,\"texCoord\":1,"
              "\"extensions\":{\"KHR_texture_transform\":{\"texCoord\":0}}}}}",
              overridden),
        "a reference whose extension names a uv set reads");
  if (overridden.Materials().size() == 1) {
    CHECK(overridden.Materials()[0].BaseColour.TexCoord == 0,
          "the extension's texCoord OVERRIDES the textureInfo's own, which is the extension's word "
          "for it -- a reader that preferred the outer field reads set 1 here");
  }
  Document kept;
  CHECK(Reads("{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0,\"texCoord\":1,"
              "\"extensions\":{\"KHR_texture_transform\":{\"offset\":[0.5,0.5]}}}}}",
              kept),
        "a reference whose extension names no uv set reads");
  if (kept.Materials().size() == 1) {
    CHECK(kept.Materials()[0].BaseColour.TexCoord == 1,
          "and where the extension supplies none the textureInfo's own set survives, so the "
          "override is not a reset to zero");
  }

  /* A PRESENT VALUE THAT IS NOT WHAT THE EXTENSION DEFINES IS REFUSED AND NEVER DEFAULTED, the same
   * rule `emissiveStrength` already carries: `Num(def)` answers the default for a string and an
   * object alike, so a silent default here would be a picture nobody could trace to the file. */
  const struct {
    const char *Spelling;
    const char *What;
  } malformed[] = {
      {"{\"index\":0,\"extensions\":{\"KHR_texture_transform\":true}}", "the extension as a boolean"},
      {"{\"index\":0,\"extensions\":{\"KHR_texture_transform\":{\"offset\":0.5}}}",
       "an offset that is not an array"},
      {"{\"index\":0,\"extensions\":{\"KHR_texture_transform\":{\"offset\":[0.5]}}}",
       "an offset of one component"},
      {"{\"index\":0,\"extensions\":{\"KHR_texture_transform\":{\"offset\":[\"0.5\",0]}}}",
       "an offset component spelled as a string"},
      {"{\"index\":0,\"extensions\":{\"KHR_texture_transform\":{\"scale\":[1,\"2\"]}}}",
       "a scale component spelled as a string"},
      {"{\"index\":0,\"extensions\":{\"KHR_texture_transform\":{\"rotation\":\"0.3\"}}}",
       "a rotation spelled as a string"},
      {"{\"index\":0,\"extensions\":{\"KHR_texture_transform\":{\"texCoord\":\"1\"}}}",
       "a texCoord spelled as a string"},
  };
  for (const auto &row : malformed) {
    Document refused;
    CHECK(!Reads(std::string("{\"pbrMetallicRoughness\":{\"baseColorTexture\":") + row.Spelling +
                     "}}",
                 refused),
          (std::string("a material declaring ") + row.What +
           " is REFUSED by name rather than read at the extension's default")
              .c_str());
    CHECK(refused.Error().find("KHR_texture_transform") != std::string::npos,
          "and the refusal names the extension, so the sentence says which field was wrong");
  }

  /* AND THE FILE THAT REQUIRES IT NOW LOADS, which is the other half of implementing an extension:
   * `TextureTransformMultiTest` lists it in `extensionsRequired`, so until this round it was refused
   * by name and no picture of it existed at all. */
  Document required;
  const std::string text =
      "{\"asset\":{\"version\":\"2.0\"},"
      "\"extensionsRequired\":[\"KHR_texture_transform\"],"
      "\"extensionsUsed\":[\"KHR_texture_transform\"]}";
  CHECK(required.Read(outshine::Span<const uint8_t>((const uint8_t *)text.data(), text.size()), ""),
        "a file that REQUIRES KHR_texture_transform is read, because this reader now implements it");
  CHECK(Document::Honours("KHR_texture_transform"),
        "and the honoured set says so, which is the one place an extension is claimed");
  return Report();
}

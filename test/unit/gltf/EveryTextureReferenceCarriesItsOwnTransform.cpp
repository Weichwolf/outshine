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

constexpr double kTolerance = 5.9604644775390625e-08;

void CarriesOffsetAndTurn(const TextureRef &reference, double offsetU, double rotationRad,
                          const char *socket) {
  using namespace outshine::Test;
  CHECK_NEAR(reference.Transform.M[2], offsetU, kTolerance, "uv",
             (std::string("the ") + socket + " reference carries ITS OWN offset, not a neighbour's")
                 .c_str());
  CHECK_NEAR(reference.Transform.M[0], std::cos(rotationRad), kTolerance, "uv per uv",
             (std::string("and its own rotation: an engine keeping one transform per material would "
                          "answer another socket's turn on the ") + socket).c_str());

  CHECK_NEAR(reference.Transform.M[3], -std::sin(rotationRad), kTolerance, "uv per uv",
             (std::string("and carries the turn with its sign on the ") + socket).c_str());
}

}

int main() {
  using namespace outshine::Test;
  Covers("I.26.13 every texture reference carries its own KHR_texture_transform, so two sockets of one material may sample one image differently");

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

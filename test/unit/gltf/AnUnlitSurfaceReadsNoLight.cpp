#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"

using outshine::Gltf::Document;
using outshine::Test::Append;
using outshine::Test::Glb;

namespace {

std::vector<uint8_t> Binary() {
  std::vector<uint8_t> bytes;
  for (const float component : {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f}) {
    Append(bytes, component);
  }
  for (const uint16_t index : {uint16_t{0}, uint16_t{1}, uint16_t{2}}) { Append(bytes, index); }
  return bytes;
}

std::string Fixture(const char *unlitDeclaration) {
  return std::string(R"({
  "asset": { "version": "2.0" },
  "extensionsUsed": [ "KHR_materials_unlit" ],
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "mesh": 0 } ],
  "buffers": [ { "byteLength": 42 } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,  "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 6 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
      "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0] },
    { "bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ],
  "materials": [
    { "name": "lit", "pbrMetallicRoughness": { "baseColorFactor": [0.25, 0.5, 0.75, 1.0] } },
    { "name": "unlit", "pbrMetallicRoughness": { "baseColorFactor": [0.1, 0.2, 0.3, 1.0] },
      "extensions": { "KHR_materials_unlit": )") +
         unlitDeclaration + R"( } }
  ],
  "meshes": [ { "primitives": [
    { "attributes": { "POSITION": 0 }, "indices": 1, "material": 0 },
    { "attributes": { "POSITION": 0 }, "indices": 1, "material": 1 }
  ] } ]
})";
}

[[nodiscard]] bool Reads(const char *unlitDeclaration, Document &out) {
  const std::vector<uint8_t> glb = Glb(Fixture(unlitDeclaration), Binary());
  return out.Read({glb.data(), glb.size()}, "unlit.glb");
}

}

int main() {
  using namespace outshine::Test;

  CHECK(Document::Honours("KHR_materials_unlit"),
        "the reader names the extension it implements, so a file that REQUIRES it is read rather "
        "than refused");

  Document document;
  const bool read = Reads("{}", document);
  CHECK(read, "a file declaring KHR_materials_unlit on one of its two materials is read");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }
  CHECK(document.Materials().size() == 2, "both materials cross");
  if (document.Materials().size() == 2) {
    CHECK(!document.Materials()[0].Surface.Unlit,
          "a material that declares no such extension is lit, which is the format's default and "
          "not an absence anything has to remember");
    CHECK(document.Materials()[1].Surface.Unlit,
          "a material that declares the extension is unlit, and that is what selects the arm whose "
          "output is the base colour");

    CHECK(document.Materials()[1].Surface.BaseColour[0] == 0.1f &&
              document.Materials()[1].Surface.BaseColour[1] == 0.2f &&
              document.Materials()[1].Surface.BaseColour[2] == 0.3f,
          "the unlit material's base colour crosses unchanged, because it IS the radiance");
  }

  Document mistyped;
  CHECK(!Reads("true", mistyped),
        "KHR_materials_unlit declared as anything but an object is refused rather than read as "
        "either answer");
  CHECK(mistyped.Error().find("KHR_materials_unlit") != std::string::npos,
        "and the refusal names the extension it could not read");

  Covers("I.68 the reader carries what the file declares and derives nothing: an unlit material "
         "is the file's own statement that no light enters its appearance");
  return Report();
}

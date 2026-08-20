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
  const float base[4][3] = {{1.f, 2.f, 3.f}, {4.f, 5.f, 6.f}, {7.f, 8.f, 9.f}, {10.f, 11.f, 12.f}};
  for (const auto &element : base) {
    for (float component : element) { Append(bytes, component); }
  }
  Append(bytes, uint16_t{1});
  Append(bytes, uint16_t{3});
  const float overrides[2][3] = {{-1.f, -2.f, -3.f}, {-4.f, -5.f, -6.f}};
  for (const auto &element : overrides) {
    for (float component : element) { Append(bytes, component); }
  }
  return bytes;
}

const char *const kJson = R"({
  "asset": { "version": "2.0" },
  "buffers": [ { "byteLength": 76 } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,  "byteLength": 48 },
    { "buffer": 0, "byteOffset": 48, "byteLength": 4 },
    { "buffer": 0, "byteOffset": 52, "byteLength": 24 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
      "sparse": { "count": 2,
        "indices": { "bufferView": 1, "componentType": 5123 },
        "values": { "bufferView": 2 } } },
    { "componentType": 5126, "count": 4, "type": "VEC3",
      "sparse": { "count": 2,
        "indices": { "bufferView": 1, "componentType": 5123 },
        "values": { "bufferView": 2 } } }
  ]
})";

}

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> binary = Binary();
  CHECK(binary.size() == 76, "the fixture's binary chunk is the length its buffer declares");
  const std::vector<uint8_t> glb = Glb(kJson, binary);

  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "sparse.glb");
  CHECK(read, "a file whose only accessors are sparse is read");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }
  CHECK(document.Accessors().size() == 2 && document.Accessors()[0].HasSparse &&
            document.Accessors()[1].HasSparse,
        "both accessors carry their sparse declaration");

  const double overBase[12] = {1, 2, 3, -1, -2, -3, 7, 8, 9, -4, -5, -6};
  std::vector<double> got;
  CHECK(document.ReadElements(0, got), "the sparse accessor over a base view decodes");
  bool agrees = got.size() == 12;
  for (size_t i = 0; i < got.size() && i < 12; ++i) { agrees = agrees && got[i] == overBase[i]; }
  CHECK(agrees, "elements 1 and 3 are overridden and 0 and 2 keep the base run");

  const double overZero[12] = {0, 0, 0, -1, -2, -3, 0, 0, 0, -4, -5, -6};
  CHECK(document.ReadElements(1, got), "the sparse accessor with no bufferView decodes");
  agrees = got.size() == 12;
  for (size_t i = 0; i < got.size() && i < 12; ++i) { agrees = agrees && got[i] == overZero[i]; }
  CHECK(agrees, "an absent bufferView is a run of zeros the overrides are written into, not an empty run");

  Note("elements overridden", 2.0, "of 4");
  Covers("I.26.6 sparse accessors, both with and without a base bufferView");
  return Report();
}

#include <string>
#include <vector>

#include "Check.h"
#include <cstring>

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
  {
    // spec 3.6.2.3: a viewless accessor WITHOUT sparse is zeros -- refusing it was a
    // deviation the audit never recorded (board:1736)
    const char *plain =
        R"({"asset":{"version":"2.0"},
            "buffers":[{"byteLength":8}],
            "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8}],
            "accessors":[{"componentType":5126,"count":6,"type":"VEC3"}]})";
    std::vector<uint8_t> eight(8, 1);
    const std::vector<uint8_t> zeroed = Glb(plain, eight);
    Document still;
    CHECK(still.Read({zeroed.data(), zeroed.size()}, "zeros.glb"),
          "**A VIEWLESS ACCESSOR WITHOUT SPARSE READS**: the spec says zeros, and no "
          "override is required (board:1736)");
    std::vector<double> flat;
    CHECK(still.ReadElements(0, flat) && flat.size() == 18,
          "and decodes to its count of zero elements");
    double sum = 0.0;
    for (const double v : flat) { sum += v; }
    CHECK(sum == 0.0, "all of them zero");
  }
  {
    // the legal LARGE viewless-sparse morph shape: a big zero field over a tiny override
    // decodes -- the element-vs-bytes bound refused exactly this (board:1736)
    const char *morph =
        R"({"asset":{"version":"2.0"},
            "buffers":[{"byteLength":16}],
            "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":4},
                           {"buffer":0,"byteOffset":4,"byteLength":12}],
            "accessors":[{"componentType":5126,"count":40,"type":"SCALAR",
              "sparse":{"count":1,
                        "indices":{"bufferView":0,"byteOffset":0,"componentType":5125},
                        "values":{"bufferView":1,"byteOffset":0,"componentType":5126}}}]})";
    std::vector<uint8_t> carried(16, 0);
    carried[0] = 3;
    const float lift = 2.5f;
    std::memcpy(carried.data() + 4, &lift, sizeof lift);
    const std::vector<uint8_t> big = Glb(morph, carried);
    Document target;
    CHECK(target.Read({big.data(), big.size()}, "morph.glb"),
          "a forty-element viewless-sparse accessor over sixteen carried bytes reads");
    std::vector<double> field;
    CHECK(target.ReadElements(0, field) && field.size() == 40 && field[3] == 2.5 &&
              field[0] == 0.0,
          "**THE LEGAL LARGE VIEWLESS-SPARSE SHAPE DECODES**: forty zeros, one lifted -- "
          "the bound is on the OUTPUT against carried bytes, sixteen-fold, named "
          "(board:1736)");
  }

  Covers("I.26.6 sparse accessors, with and without a base bufferView, the plain viewless "
         "zero field, and the legal large morph shape under the named output bound "
         "(board:1736)");
  return Report();
}

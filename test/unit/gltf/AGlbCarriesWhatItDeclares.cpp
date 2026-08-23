#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"
#include "Emit.h"

using outshine::Gltf::ComponentType;
using outshine::Gltf::Document;
using outshine::Gltf::ElementType;
using outshine::Gltf::PrimitiveMode;
using outshine::Test::Append;
using outshine::Test::Glb;
using outshine::Test::PadTo4;

namespace {

constexpr float kPosition[4][3] = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 0.f}};
constexpr int8_t kNormal[4][3] = {{0, 0, 127}, {-127, 0, 0}, {0, 127, 0}, {0, 0, -128}};
constexpr uint16_t kTexcoord[4][2] = {{0, 0}, {65535, 0}, {65535, 65535}, {0, 65535}};
constexpr uint32_t kIndex[6] = {0, 1, 2, 0, 2, 3};

std::vector<uint8_t> Binary() {
  std::vector<uint8_t> bytes;
  for (const auto &vertex : kPosition) {
    Append(bytes, vertex[0]);
    Append(bytes, vertex[1]);
    Append(bytes, vertex[2]);
    Append(bytes, uint32_t{0xCDCDCDCD});
  }
  for (const auto &normal : kNormal) {
    for (int8_t component : normal) { Append(bytes, component); }
  }
  for (const auto &uv : kTexcoord) {
    Append(bytes, uv[0]);
    Append(bytes, uv[1]);
  }
  for (uint32_t index : kIndex) { Append(bytes, static_cast<uint8_t>(index)); }
  PadTo4(bytes, 0);
  for (uint32_t index : kIndex) { Append(bytes, static_cast<uint16_t>(index)); }
  for (uint32_t index : kIndex) { Append(bytes, index); }
  return bytes;
}

const char *const kJson = R"({
  "asset": { "version": "2.0" },
  "extensionsUsed": [ "KHR_mesh_quantization" ],
  "extensionsRequired": [ "KHR_mesh_quantization" ],
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "mesh": 0 } ],
  "buffers": [ { "byteLength": 136 } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,   "byteLength": 64, "byteStride": 16 },
    { "buffer": 0, "byteOffset": 64,  "byteLength": 12 },
    { "buffer": 0, "byteOffset": 76,  "byteLength": 16 },
    { "buffer": 0, "byteOffset": 92,  "byteLength": 6 },
    { "buffer": 0, "byteOffset": 100, "byteLength": 12 },
    { "buffer": 0, "byteOffset": 112, "byteLength": 24 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
      "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0] },
    { "bufferView": 1, "componentType": 5120, "count": 4, "type": "VEC3", "normalized": true },
    { "bufferView": 2, "componentType": 5123, "count": 4, "type": "VEC2", "normalized": true },
    { "bufferView": 3, "componentType": 5121, "count": 6, "type": "SCALAR" },
    { "bufferView": 4, "componentType": 5123, "count": 6, "type": "SCALAR" },
    { "bufferView": 5, "componentType": 5125, "count": 6, "type": "SCALAR" }
  ],
  "meshes": [ { "primitives": [
    { "mode": 0, "attributes": { "POSITION": 0 } },
    { "mode": 1, "attributes": { "POSITION": 0 } },
    { "mode": 2, "attributes": { "POSITION": 0 } },
    { "mode": 3, "attributes": { "POSITION": 0 } },
    { "mode": 4, "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }, "indices": 4 },
    { "mode": 5, "attributes": { "POSITION": 0 }, "indices": 3 },
    { "mode": 6, "attributes": { "POSITION": 0 }, "indices": 5 }
  ] } ]
})";

}

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> binary = Binary();
  CHECK(binary.size() == 136, "the fixture's binary chunk is the length its buffer declares");
  const std::vector<uint8_t> glb = Glb(kJson, binary);

  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "fixture.glb");
  CHECK(read, "a GLB with a JSON chunk and a binary chunk is read");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }
  CHECK(document.Version() == "2.0", "the asset version crosses");
  CHECK(document.DefaultScene() == 0, "the default scene is the one the file names");
  CHECK(document.Meshes().size() == 1, "one mesh crosses");
  CHECK(document.Nodes().size() == 1 && document.Nodes()[0].Mesh == 0,
        "the node still names its mesh");

  const auto &primitives = document.Meshes()[0].Primitives;
  CHECK(primitives.size() == 7, "all seven primitive modes cross as seven primitives");
  for (size_t i = 0; i < primitives.size() && i < 7; ++i) {
    CHECK(static_cast<int>(primitives[i].Mode) == static_cast<int>(i),
          "each primitive keeps the mode its file states, in order");
  }

  std::vector<double> positions;
  CHECK(document.ReadElements(0, positions), "the position accessor decodes");
  CHECK(positions.size() == 12, "four VEC3 positions are twelve components");
  double lowest[3] = {1e30, 1e30, 1e30};
  double highest[3] = {-1e30, -1e30, -1e30};
  bool positionsAgree = positions.size() == 12;
  for (size_t vertex = 0; vertex < 4 && positions.size() == 12; ++vertex) {
    for (size_t axis = 0; axis < 3; ++axis) {
      const double got = positions[vertex * 3 + axis];
      positionsAgree = positionsAgree && got == static_cast<double>(kPosition[vertex][axis]);
      lowest[axis] = (got < lowest[axis]) ? got : lowest[axis];
      highest[axis] = (got > highest[axis]) ? got : highest[axis];
    }
  }
  CHECK(positionsAgree,
        "every position decodes exactly, so the 16-byte stride was stepped and not the 12-byte "
        "element");
  const auto &bounds = document.Accessors()[0];
  bool boundsAgree = bounds.Min.size() == 3 && bounds.Max.size() == 3;
  for (size_t axis = 0; axis < 3 && boundsAgree; ++axis) {
    boundsAgree = boundsAgree && bounds.Min[axis] == lowest[axis] && bounds.Max[axis] == highest[axis];
  }
  CHECK(boundsAgree, "the decoded vertices reach exactly the bounds the accessor declares");

  std::vector<double> normals;
  CHECK(document.ReadElements(1, normals), "the normalised signed-byte normals decode");
  if (normals.size() == 12) {
    CHECK_NEAR(normals[2], 1.0, 1e-12, "unit", "127 of a normalised byte is +1");
    CHECK_NEAR(normals[3], -1.0, 1e-12, "unit", "-127 of a normalised byte is -1");
    CHECK_NEAR(normals[11], -1.0, 1e-12, "unit",
               "-128 clamps to -1: a signed component divides by its positive maximum");
  }

  std::vector<double> texcoords;
  CHECK(document.ReadElements(2, texcoords), "the normalised unsigned-short texcoords decode");
  if (texcoords.size() == 8) {
    CHECK_NEAR(texcoords[0], 0.0, 1e-12, "unit", "0 of a normalised unsigned short is 0");
    CHECK_NEAR(texcoords[2], 1.0, 1e-12, "unit", "65535 of a normalised unsigned short is 1");
  }

  for (int accessor = 3; accessor <= 5; ++accessor) {
    std::vector<uint32_t> indices;
    CHECK(document.ReadIndices(accessor, indices), "an index accessor decodes at every width");
    bool same = indices.size() == 6;
    for (size_t i = 0; i < indices.size() && i < 6; ++i) { same = same && indices[i] == kIndex[i]; }
    CHECK(same, "u8, u16 and u32 indices decode to the same six values");
  }

  Note("vertices", 4.0, "count");
  Note("primitive modes", static_cast<double>(primitives.size()), "count");
  {
    // the container's ceiling, proven without four gibibytes of fixture: the bound is a
    // free function the writer holds before its casts
    using outshine::Gltf::GlbFits;
    CHECK(GlbFits(1024, 1024), "a small subject fits");
    CHECK(!GlbFits(1024, 5ull * 1024 * 1024 * 1024),
          "**A SUBJECT PAST 4 GiB REFUSES INSTEAD OF EMITTING A HEADER THAT LIES** -- the "
          "GLB container is 32-bit and the writer holds the cap before the cast "
          "(board:1728)");
    CHECK(!GlbFits(0xffffffffu - 20, 8) && !GlbFits(8, 0xffffffffu - 20),
          "and the rim just under the ceiling accounts for the three headers -- no "
          "underflow arithmetic passes it");
    CHECK(!GlbFits(3ull * 1024 * 1024 * 1024, 3ull * 1024 * 1024 * 1024),
          "two halves that each fit still refuse together");
  }

  Covers("I.26 the reader: containers, buffers, bufferViews, accessors, meshes and primitives");
  return Report();
}

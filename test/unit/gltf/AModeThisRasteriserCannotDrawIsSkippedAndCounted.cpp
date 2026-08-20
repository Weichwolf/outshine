#include <cctype>
#include <string>
#include <vector>

#include "Check.h"

#include "Glb.h"

#include "Document.h"
#include "Subject.h"

using outshine::Gltf::Document;
using outshine::Gltf::PrimitiveMode;
using outshine::Gltf::Subject;
using outshine::Test::Glb;

namespace {

const char *const kMixed = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0, 1, 2, 3] } ],
  "nodes": [
    { "name": "points",    "mesh": 0 },
    { "name": "lines",     "mesh": 1 },
    { "name": "line-loop", "mesh": 2 },
    { "name": "surface",   "mesh": 3 }
  ],
  "meshes": [
    { "primitives": [ { "attributes": { "POSITION": 0 }, "mode": 0 } ] },
    { "primitives": [ { "attributes": { "POSITION": 0 }, "mode": 1 } ] },
    { "primitives": [ { "attributes": { "POSITION": 0 }, "mode": 2 } ] },
    { "primitives": [ { "attributes": { "POSITION": 1 }, "mode": 4 } ] }
  ],
  "buffers": [ { "byteLength": 48 } ],
  "bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 48 } ],
  "accessors": [ { "bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
                   "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0] },
                 { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                   "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0] } ]
})";

const char *const kNoSurface = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "name": "points", "mesh": 0 } ],
  "meshes": [ { "primitives": [ { "attributes": { "POSITION": 0 }, "mode": 0 } ] } ],
  "buffers": [ { "byteLength": 48 } ],
  "bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 48 } ],
  "accessors": [ { "bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
                   "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0] } ]
})";

std::vector<uint8_t> Quad() {
  const float xyz[12] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                         0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(xyz);
  return std::vector<uint8_t>(bytes, bytes + sizeof(xyz));
}

}

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> mixed = Glb(kMixed, Quad());
  Document document;
  const bool read = document.Read({mixed.data(), mixed.size()}, "modes.glb");
  CHECK(read, "a file mixing surface and non-surface modes reads");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }

  Subject subject;
  const bool built = subject.Build(document, {});
  CHECK(built, "THE SUBJECT FLATTENS RATHER THAN REFUSING -- three of its four primitives are modes "
               "this rasteriser has no pass for, and the fourth is a surface it does");
  if (!built) {
    std::printf("       %s\n", subject.Error().c_str());
    return Report();
  }

  Note("parts", (double)subject.Parts().size(), "parts");
  Note("primitives not drawn", (double)subject.NotDrawn().Primitives, "primitives");

  CHECK(subject.Parts().size() == 1, "the one surface primitive drew");
  CHECK(!subject.Parts().empty() && subject.Parts()[0].NodeName == "surface",
        "and it is the surface node, not a point cloud that happened to survive");

  CHECK(subject.NotDrawn().Primitives == 3,
        "WHAT WAS NOT DRAWN IS A NUMBER THE CALLER CAN READ -- a subject that dropped three "
        "primitives and one that dropped none are otherwise the same object, and telling them apart "
        "is the whole of what a capability statement is for");
  CHECK(subject.NotDrawn().ByMode[(size_t)PrimitiveMode::Points] == 1,
        "the count says WHICH modes were dropped, so a caller can tell a point cloud from a "
        "polyline without re-reading the file");
  CHECK(subject.NotDrawn().ByMode[(size_t)PrimitiveMode::Lines] == 1, "LINES is counted as itself");
  CHECK(subject.NotDrawn().ByMode[(size_t)PrimitiveMode::LineLoop] == 1,
        "LINE_LOOP is counted as itself and not folded into LINES");
  CHECK(subject.NotDrawn().ByMode[(size_t)PrimitiveMode::Triangles] == 0,
        "and a mode that DID draw is not counted among the dropped, which is the direction a "
        "miscount would most easily go");

  const std::vector<uint8_t> bare = Glb(kNoSurface, Quad());
  Document only;
  const bool readBare = only.Read({bare.data(), bare.size()}, "points-only.glb");
  CHECK(readBare, "a file whose every primitive is POINTS still reads");
  if (readBare) {
    Subject nothing;
    const bool drew = nothing.Build(only, {});
    CHECK(!drew, "A SUBJECT WITH NO SURFACE AT ALL IS STILL A REFUSAL -- skipping what cannot be "
                 "drawn must not become drawing nothing quietly");
    if (!drew) { std::printf("NOTE refused: %s\n", nothing.Error().c_str()); }
  }

  Covers("a primitive mode this rasteriser has no pass for is skipped and counted, the "
         "surfaces beside it still draw, and a subject with no surface at all is refused");
  return Report();
}

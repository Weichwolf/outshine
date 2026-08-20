#include <string>
#include <vector>

#include "Check.h"

#include "Glb.h"

#include "Document.h"

using outshine::Gltf::Document;
using outshine::Test::Glb;

namespace {

std::string File(bool required) {
  const std::string requires_ =
      required ? "\"extensionsRequired\": [\"KHR_mesh_quantization\"],\n"
                 "  \"extensionsUsed\": [\"KHR_mesh_quantization\"],\n  "
               : "";
  return "{\n  \"asset\": { \"version\": \"2.0\" },\n  " + requires_ +
         R"("scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "mesh": 0 } ],
  "meshes": [ { "primitives": [ { "attributes": { "POSITION": 0 } } ] } ],
  "buffers": [ { "byteLength": 24 } ],
  "bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 24 } ],
  "accessors": [ { "bufferView": 0, "componentType": 5122, "count": 3, "type": "VEC3",
                   "min": [0, 0, 0], "max": [32767, 32767, 0] } ]
})";
}

const char *const kFloatJoints = R"({
  "asset": { "version": "2.0" },
  "extensionsRequired": [ "KHR_mesh_quantization" ],
  "extensionsUsed": [ "KHR_mesh_quantization" ],
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "mesh": 0 } ],
  "meshes": [ { "primitives": [ { "attributes": { "POSITION": 0, "JOINTS_0": 1 } } ] } ],
  "buffers": [ { "byteLength": 84 } ],
  "bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
                   { "buffer": 0, "byteOffset": 36, "byteLength": 48 } ],
  "accessors": [ { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                   "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0] },
                 { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC4" } ]
})";

std::vector<uint8_t> Bytes(size_t count) { return std::vector<uint8_t>(count, 0); }

}

int main() {
  using namespace outshine::Test;

  {
    const std::string json = File(false);
    const std::vector<uint8_t> glb = Glb(json, Bytes(24));
    Document document;
    const bool read = document.Read({glb.data(), glb.size()}, "unquantised.glb");
    CHECK(!read, "A QUANTISED POSITION WITHOUT THE EXTENSION IS REFUSED -- base glTF 2.0 states "
                 "POSITION is VEC3 float, and a file that quantises without saying so is one this "
                 "reader would otherwise have decoded into the wrong geometry and called fine");
    if (!read) { std::printf("NOTE refused: %s\n", document.Error().c_str()); }
  }

  {
    const std::string json = File(true);
    const std::vector<uint8_t> glb = Glb(json, Bytes(24));
    Document document;
    const bool read = document.Read({glb.data(), glb.size()}, "quantised.glb");
    CHECK(read, "and the SAME FILE with KHR_mesh_quantization required reads, which is what makes "
                "the declaration mean something rather than decorate a list");
    if (!read) { std::printf("       %s\n", document.Error().c_str()); }
  }

  {
    const std::vector<uint8_t> glb = Glb(kFloatJoints, Bytes(84));
    Document document;
    const bool read = document.Read({glb.data(), glb.size()}, "float-joints.glb");
    CHECK(!read, "THE EXTENSION WIDENS FOUR ROWS AND NOT THE TABLE -- POSITION, NORMAL, TANGENT and "
                 "TEXCOORD_n, so a float JOINTS_0 is refused even with the extension required, and "
                 "declaring it is not a licence for every shape");
    if (!read) { std::printf("NOTE refused: %s\n", document.Error().c_str()); }
  }

  Covers("KHR_mesh_quantization: the attribute table is the format's, the extension "
         "widens exactly four of its rows, and a file that quantises without requiring it is refused");
  return Report();
}

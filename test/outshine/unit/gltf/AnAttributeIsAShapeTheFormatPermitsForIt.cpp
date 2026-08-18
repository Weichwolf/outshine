/* THE ATTRIBUTE TABLE IS THE FORMAT'S AND `KHR_mesh_quantization` IS WHAT WIDENS IT (board:1384).
 *
 * Base glTF 2.0 says a POSITION is `VEC3 float` and nothing else. This reader decoded every component
 * type generically, so a quantised POSITION already worked -- which meant honouring the extension
 * would have been a name added to a list and no behaviour built. **The check IS the behaviour**: with
 * it, declaring the extension means something, and without it a file that quantises WITHOUT declaring
 * the extension is a file this reader would have drawn wrongly and called fine.
 *
 * WHY IT MUST BE REQUIRED AND NOT MERELY USED, in the extension's own words: *because the extension
 * does not provide a way to specify both FLOAT and quantized versions of the data, files that use the
 * extension must specify it in extensionsRequired - the extension is not optional.* A reader that
 * skipped it would not draw a plainer picture; it would draw the wrong geometry.
 *
 * [MEASURED] over the 148 models at the pin, this check refuses NONE of them: one declares the
 * extension and uses it, and no model uses a non-base combination without declaring it. That number
 * is why the check could land without a case going red, and it was taken before the code was written. */
#include <string>
#include <vector>

#include "Check.h"

#include "Glb.h"

#include "Document.h"

using outshine::Gltf::Document;
using outshine::Test::Glb;

namespace {

/* `componentType` 5122 is SHORT. A POSITION of shorts is the extension's whole point and base glTF's
 * plain refusal, so one file with the declaration and one without decide the rule between them. */
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

/* A JOINTS_0 of floats, which NEITHER base glTF nor the quantization extension permits -- the
 * extension widens POSITION, NORMAL, TANGENT and TEXCOORD_n and leaves every other row alone. */
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

} // namespace

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

  Covers("board:1384 KHR_mesh_quantization: the attribute table is the format's, the extension "
         "widens exactly four of its rows, and a file that quantises without requiring it is refused");
  return Report();
}

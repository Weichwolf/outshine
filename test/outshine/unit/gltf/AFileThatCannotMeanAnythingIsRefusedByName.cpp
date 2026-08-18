/* WHAT THE READER WILL NOT INTERPRET, AND THE SENTENCE IT SAYS SO WITH. Every claim here asserts the
 * refusal AND a fragment of its wording: a reader that refused everything for one reason would pass a
 * test that only counted `false`, and that is the vacuous shape this repository keeps finding. */
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"

using outshine::Gltf::Document;
using outshine::Test::Glb;

namespace {

const std::vector<uint8_t> kFourBytes = {0, 0, 0, 0};

/* Every subject is a whole file, because a refusal is a property of a file and not of a fragment. */
struct Refusal {
  const char *Why;
  const char *Says;
  std::string Json;
  std::vector<uint8_t> Binary = {};
  bool AsGlb = true;
};

void Holds(const Refusal &subject) {
  using namespace outshine::Test;
  const std::vector<uint8_t> bytes =
      subject.AsGlb ? Glb(subject.Json, subject.Binary)
                    : std::vector<uint8_t>(subject.Json.begin(), subject.Json.end());
  Document document;
  const bool read = document.Read({bytes.data(), bytes.size()}, "subject.glb");
  CHECK(!read, subject.Why);
  const bool named = document.Error().find(subject.Says) != std::string::npos;
  CHECK(named, "the refusal names what it refused");
  if (read || !named) {
    std::printf("       expected '%s', got '%s'\n", subject.Says, document.Error().c_str());
  }
}

} // namespace

int main() {
  using namespace outshine::Test;

  Document empty;
  CHECK(!empty.Read({}, "nothing.glb"), "no bytes at all is refused");
  CHECK(empty.Error().find("is empty") != std::string::npos, "and the refusal says so");

  Document absent;
  CHECK(!absent.ReadFile("test/outshine/unit/gltf/there-is-no-such-file.glb"),
        "a path that names no file is refused");
  CHECK(absent.Error().find("cannot be opened") != std::string::npos,
        "and the refusal carries the path");

  Holds({"a glTF 1.0 asset is refused", "declares asset.version '1.0'",
         R"({"asset":{"version":"1.0"}})"});
  Holds({"an accessor with a componentType the format does not define is refused",
         "componentType 5124",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":4}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":4}],
             "accessors":[{"bufferView":0,"componentType":5124,"count":1,"type":"SCALAR"}]})", kFourBytes});
  Holds({"an accessor whose element type the format does not define is refused", "type 'VEC5'",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":4}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":4}],
             "accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"VEC5"}]})", kFourBytes});
  Holds({"a normalized float accessor is refused", "normalized float",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":4}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":4}],
             "accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"SCALAR",
                           "normalized":true}]})", kFourBytes});
  Holds({"a bufferView that runs past its buffer is refused", "of a buffer of 4 bytes",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":4}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":64}]})", kFourBytes});
  Holds({"a node carrying both a matrix and a TRS component is refused",
         "both a matrix and a TRS component",
         R"({"asset":{"version":"2.0"},
             "nodes":[{"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],"translation":[1,2,3]}]})"});
  Holds({"a node that is the child of two nodes is refused", "a glTF hierarchy is a forest",
         R"({"asset":{"version":"2.0"},
             "nodes":[{"children":[2]},{"children":[2]},{}]})"});
  Holds({"a primitive naming an accessor the file does not carry is refused",
         "attribute POSITION names an accessor the file does not carry",
         R"({"asset":{"version":"2.0"},
             "meshes":[{"primitives":[{"attributes":{"POSITION":7}}]}]})"});
  Holds({"a primitive mode the format does not define is refused", "mode glTF 2.0 does not define",
         R"({"asset":{"version":"2.0"},
             "meshes":[{"primitives":[{"mode":7,"attributes":{}}]}]})"});
  Holds({"a camera that is neither perspective nor orthographic is refused", "glTF 2.0 has two",
         R"({"asset":{"version":"2.0"},"cameras":[{"type":"panoramic"}]})"});
  /* A named refusal rather than a gap: this reader decodes two containers, and a file that carries
   * its buffer as base64 must say which one it is instead of reading as an empty mesh. */
  Holds({"an embedded data: buffer is refused by name", "data: URI, which this reader does not decode",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":4,"uri":"data:application/octet-stream;base64,AAAAAA=="}]})"});
  Holds({"a .gltf whose external buffer is not beside it is refused", "which cannot be opened",
         R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":4,"uri":"nowhere.bin"}]})",
         {}, false});

  /* THE TWO DOORS A FILE STATES ITS OWN UNREADABILITY THROUGH, and both stood open until this round.
   * A quantized or Draco-compressed file whose required extension is ignored does not fail to read:
   * it reads as plausible numbers in the wrong units, which is the worst answer available. */
  Holds({"an asset.minVersion above 2.0 is refused", "asset.minVersion '2.1'",
         R"({"asset":{"version":"2.0","minVersion":"2.1"}})"});
  Holds({"a required extension this reader does not implement is refused",
         "requires extension 'KHR_draco_mesh_compression'",
         R"({"asset":{"version":"2.0"},
             "extensionsRequired":["KHR_draco_mesh_compression"]})"});
  Holds({"and so is a required extension named beside one that is merely used",
         "requires extension 'KHR_materials_pbrSpecularGlossiness'",
         R"({"asset":{"version":"2.0"},
             "extensionsUsed":["KHR_texture_transform","KHR_materials_pbrSpecularGlossiness"],
             "extensionsRequired":["KHR_materials_pbrSpecularGlossiness"]})"});
  /* THE EXAMPLE MUST BE AN EXTENSION THIS READER GENUINELY DOES NOT HONOUR, and it has moved twice
   * for the one legitimate reason: **the test did not become wrong, its SUBJECT did**. It was
   * `KHR_mesh_quantization` until `board:1384` built that, then `KHR_materials_sheen` until
   * `board:1385` built that -- and the second line said, in as many words, that it was expected to
   * move again.
   *
   * IT IS `KHR_materials_pbrSpecularGlossiness` NOW, AND THAT ONE CANNOT MOVE. Khronos ARCHIVED it,
   * and an obsolete extension is one this engine does not implement by the owner's own ruling -- so
   * this is the first subject here that is stable by construction rather than until somebody gets to
   * it. *A specification that has to be edited every time the engine gains a capability was telling
   * the truth about the engine and lying about itself.* */

  /* THE APPEARANCE TABLES REFUSE BY NAME TOO, on the same rule: a dangling index that read as -1
   * would draw an untextured surface and look like a material somebody authored that way. */
  Holds({"a texture naming an image the file does not carry is refused", "names image 3 of 0",
         R"({"asset":{"version":"2.0"},"textures":[{"source":3}]})"});
  Holds({"a material naming a texture the file does not carry is refused",
         "baseColorTexture names texture 2 of 0",
         R"({"asset":{"version":"2.0"},
             "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":2}}}]})"});
  Holds({"an alphaMode the format does not define is refused", "alphaMode 'DITHER'",
         R"({"asset":{"version":"2.0"},"materials":[{"alphaMode":"DITHER"}]})"});
  /* A DECLARED VALUE THAT IS NOT OF ITS DECLARED TYPE IS THE SILENT-SUCCESS SHAPE, not a parse
   * error: the JSON is well formed and the reader's `Num(default)` answers the default for a string
   * as readily as for an absent key, so the two claims below are what separate "we read 1" from
   * "the file said 1". */
  Holds({"an emissiveStrength that is not a number is refused", "emissiveStrength that is not a number",
         R"({"asset":{"version":"2.0"},"materials":[{"extensions":
             {"KHR_materials_emissive_strength":{"emissiveStrength":"2"}}}]})"});
  Holds({"a negative emissiveStrength is refused", "the extension's minimum is 0",
         R"({"asset":{"version":"2.0"},"materials":[{"extensions":
             {"KHR_materials_emissive_strength":{"emissiveStrength":-1}}}]})"});
  Holds({"an image that is neither a uri nor a bufferView is refused",
         "has neither a uri nor a bufferView",
         R"({"asset":{"version":"2.0"},"images":[{"name":"nowhere"}]})"});
  Holds({"a primitive naming a material the file does not carry is refused", "names material 1 of 0",
         R"({"asset":{"version":"2.0"},
             "meshes":[{"primitives":[{"attributes":{},"material":1}]}]})"});

  /* A GLB carrying a binary chunk and no JSON chunk at all, assembled by hand because the fixture
   * always writes one. */
  std::vector<uint8_t> noJson;
  Append(noJson, uint32_t{0x46546C67});
  Append(noJson, uint32_t{2});
  Append(noJson, uint32_t{28});
  Append(noJson, uint32_t{8});
  Append(noJson, uint32_t{0x004E4942});
  Append(noJson, uint64_t{0});
  Document headless;
  CHECK(!headless.Read({noJson.data(), noJson.size()}, "headless.glb"),
        "a GLB with no JSON chunk is refused");
  CHECK(headless.Error().find("no JSON chunk") != std::string::npos,
        "and the refusal says which chunk was missing");

  /* A GLB of another version, assembled by hand because the fixture only writes version 2. */
  std::vector<uint8_t> wrongVersion;
  Append(wrongVersion, uint32_t{0x46546C67});
  Append(wrongVersion, uint32_t{1});
  Append(wrongVersion, uint32_t{12});
  Document old;
  CHECK(!old.Read({wrongVersion.data(), wrongVersion.size()}, "old.glb"),
        "a GLB of version 1 is refused");
  CHECK(old.Error().find("GLB of version 1") != std::string::npos,
        "and the refusal names the version it found");

  /* A decode that cannot be satisfied is a false return, not a short buffer of zeros. */
  const char *const overrun = R"({"asset":{"version":"2.0"},
      "buffers":[{"byteLength":4}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":4}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":8,"type":"VEC3"}]})";
  const std::vector<uint8_t> tooShort = Glb(overrun, {0, 0, 0, 0});
  Document counted;
  CHECK(counted.Read({tooShort.data(), tooShort.size()}, "short.glb"),
        "an accessor whose count outruns its view is a decode failure, not a parse failure");
  std::vector<double> nothing{1.0, 2.0};
  CHECK(!counted.ReadElements(0, nothing),
        "reading it refuses rather than returning a short run");
  CHECK(nothing.empty(), "and leaves nothing behind that a caller could mistake for data");

  Covers("I.26 a file that does not carry what a case needs is a refusal that names what was missing");
  return Report();
}

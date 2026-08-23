#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"

using outshine::Gltf::Document;
using outshine::Test::Glb;

namespace {

const std::vector<uint8_t> kFourBytes = {0, 0, 0, 0};

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

}

int main() {
  using namespace outshine::Test;

  Document empty;
  CHECK(!empty.Read({}, "nothing.glb"), "no bytes at all is refused");
  CHECK(empty.Error().find("is empty") != std::string::npos, "and the refusal says so");

  Document absent;
  CHECK(!absent.ReadFile("test/unit/gltf/there-is-no-such-file.glb"),
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
  Holds({"a two-node cycle is refused instead of hanging the scene walk",
         "the chain of parents is a cycle",
         R"({"asset":{"version":"2.0"},
             "nodes":[{"children":[1]},{"children":[0]}]})"});
  Holds({"a self-child is refused by the same rule", "the chain of parents is a cycle",
         R"({"asset":{"version":"2.0"},"nodes":[{"children":[0]}]})"});
  Holds({"a scene root that has a parent is refused -- the spec's scene nodes are roots",
         "scene nodes are root nodes",
         R"({"asset":{"version":"2.0"},
             "nodes":[{"children":[1]},{}],"scenes":[{"nodes":[1]}]})"});
  Holds({"a primitive naming an accessor the file does not carry is refused",
         "attribute POSITION names an accessor the file does not carry",
         R"({"asset":{"version":"2.0"},
             "meshes":[{"primitives":[{"attributes":{"POSITION":7}}]}]})"});
  Holds({"a primitive mode the format does not define is refused", "mode glTF 2.0 does not define",
         R"({"asset":{"version":"2.0"},
             "meshes":[{"primitives":[{"mode":7,"attributes":{}}]}]})"});
  Holds({"a camera that is neither perspective nor orthographic is refused", "glTF 2.0 has two",
         R"({"asset":{"version":"2.0"},"cameras":[{"type":"panoramic"}]})"});

  Holds({"an embedded data: buffer is refused by name", "data: URI, which this reader does not decode",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":4,"uri":"data:application/octet-stream;base64,AAAAAA=="}]})"});
  Holds({"a .gltf whose external buffer is not beside it is refused", "which cannot be opened",
         R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":4,"uri":"nowhere.bin"}]})",
         {}, false});

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

  Holds({"a texture naming an image the file does not carry is refused", "names image 3 of 0",
         R"({"asset":{"version":"2.0"},"textures":[{"source":3}]})"});
  Holds({"a material naming a texture the file does not carry is refused",
         "baseColorTexture names texture 2 of 0",
         R"({"asset":{"version":"2.0"},
             "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":2}}}]})"});
  Holds({"an alphaMode the format does not define is refused", "alphaMode 'DITHER'",
         R"({"asset":{"version":"2.0"},"materials":[{"alphaMode":"DITHER"}]})"});

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

  std::vector<uint8_t> wrongVersion;
  Append(wrongVersion, uint32_t{0x46546C67});
  Append(wrongVersion, uint32_t{1});
  Append(wrongVersion, uint32_t{12});
  Document old;
  CHECK(!old.Read({wrongVersion.data(), wrongVersion.size()}, "old.glb"),
        "a GLB of version 1 is refused");
  CHECK(old.Error().find("GLB of version 1") != std::string::npos,
        "and the refusal names the version it found");

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

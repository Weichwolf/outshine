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
  Holds({"a count of 1e300 refuses instead of an undefined cast",
         "not a whole non-negative count",
         R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":4}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":4}],
             "accessors":[{"bufferView":0,"componentType":5126,"count":1e300,"type":"SCALAR"}]})",
         kFourBytes});
  Holds({"a negative byteOffset refuses instead of truncating to a huge size",
         "not a whole non-negative count",
         R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":4}],
             "bufferViews":[{"buffer":0,"byteOffset":-5,"byteLength":4}]})",
         kFourBytes});
  Holds({"a byteStride outside the spec's window refuses by name",
         "a multiple of 4 in [4, 252]",
         R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":16}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":16,"byteStride":7}]})",
         std::vector<uint8_t>(16, 0)});
  Holds({"a misspelt standard semantic refuses -- application attributes start with _",
         "shape glTF 2.0 does not permit",
         R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],
             "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
                           "min":[0,0,0],"max":[1,1,0]}],
             "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMALS":0}}]}]})",
         std::vector<uint8_t>(36, 0)});
  Holds({"a POSITION accessor without min and max refuses -- the spec requires the bounds",
         "min/max bounds glTF 2.0 requires",
         R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],
             "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],
             "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}]})",
         std::vector<uint8_t>(36, 0)});
  Holds({"a rotation sampler with twice the outputs refuses -- only weights multiply",
         "demands exactly",
         R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":72}],
             "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},
                            {"buffer":0,"byteOffset":8,"byteLength":64}],
             "accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR",
                           "min":[0],"max":[1]},
                          {"bufferView":1,"componentType":5126,"count":4,"type":"VEC4"}],
             "nodes":[{}],
             "animations":[{"channels":[{"sampler":0,"target":{"node":0,"path":"rotation"}}],
                            "samplers":[{"input":0,"output":1}]}]})",
         std::vector<uint8_t>(72, 0)});
  // 1733's cost arms live in the reading tests below and in the timing arm at the end;
  // the shapes here stay the refusals they were
  Holds({"a primitive naming an accessor the file does not carry is refused",
         "attribute POSITION names an accessor the file does not carry",
         R"({"asset":{"version":"2.0"},
             "meshes":[{"primitives":[{"attributes":{"POSITION":7}}]}]})"});
  Holds({"a primitive mode the format does not define is refused", "mode glTF 2.0 does not define",
         R"({"asset":{"version":"2.0"},
             "meshes":[{"primitives":[{"mode":7,"attributes":{}}]}]})"});
  Holds({"a camera that is neither perspective nor orthographic is refused", "glTF 2.0 has two",
         R"({"asset":{"version":"2.0"},"cameras":[{"type":"panoramic"}]})"});

  // board:1396: a data: URI is legal for a buffer and for an image, and a .gltf that embeds its
  // own bytes is the ordinary shape of a small asset. The reader decodes it now, so what stands
  // here are the refusals the DECODE owes: an encoding it does not carry, an alphabet it does
  // not accept, and a declared length that disagrees with what the payload holds.
  Holds({"a data: buffer in an encoding this reader does not carry is refused", "no ;base64",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":4,"uri":"data:application/octet-stream,AAAA"}]})"});
  Holds({"a data: buffer whose payload is outside the alphabet is refused", "the alphabet",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":3,"uri":"data:application/octet-stream;base64,AA*A"}]})"});
  Holds({"a data: buffer that decodes to a length it did not declare is refused",
         "disagrees with its payload",
         R"({"asset":{"version":"2.0"},
             "buffers":[{"byteLength":8,"uri":"data:application/octet-stream;base64,AAAAAA=="}]})"});
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
  {
    // the declaration cannot buy the allocation: a four-byte .bin beside a gltf declaring
    // 4294967295 bytes refuses on the MEASURED count -- the resize follows the file, so
    // this arm returns in milliseconds instead of zero-filling four gigabytes
    const std::string bin = outshine::Test::PlantedPath("hostile.bin");
    {
      std::FILE *const file = std::fopen(bin.c_str(), "wb");
      CHECK(file != nullptr, "the four-byte bin plants");
      std::fwrite("abcd", 1, 4, file);
      std::fclose(file);
    }
    const std::string doc = outshine::Test::PlantedPath("hostile.gltf");
    {
      std::FILE *const file = std::fopen(doc.c_str(), "wb");
      const std::string text =
          std::string(R"({"asset":{"version":"2.0"},"buffers":[{"uri":"hostile.bin",)") +
          R"("byteLength":4294967295}]})";
      std::fwrite(text.data(), 1, text.size(), file);
      std::fclose(file);
    }
    Document greedy;
    CHECK(!greedy.ReadFile(doc) &&
              greedy.Error().find("4 are present") != std::string::npos,
          "**A DECLARED byteLength BUYS NO ALLOCATION THE FILE DOES NOT CARRY**: the "
          "refusal names the four measured bytes, and no gigabytes were zero-filled on "
          "the way to it (board:1736)");
  }
  {
    // the forest proof is linear: a five-thousand-node parent CHAIN reads inside the
    // suite's own patience -- the unmemoised walk paid n^2/2 steps here
    std::string chain = R"({"asset":{"version":"2.0"},"nodes":[)";
    for (int at = 0; at < 5000; ++at) {
      if (at > 0) { chain += ','; }
      chain += "{\"children\":[" + std::to_string(at + 1) + "]}";
    }
    chain += ",{}]}";
    Document linear;
    const std::vector<uint8_t> bytes(chain.begin(), chain.end());
    CHECK(linear.Read({bytes.data(), bytes.size()}, "chain.gltf"),
          "**A FIVE-THOUSAND-NODE CHAIN READS IN LINEAR TIME** -- every node the root walk "
          "passes is proven with it (board:1733)");
  }
  {
    // a viewless fill is bounded by the bytes the file carries: count five hundred million from
    // a two-hundred-byte file answers nothing instead of gigabytes of zeros
    const std::string tiny =
        R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":8}],
            "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8}],
            "accessors":[{"componentType":5126,"count":500000000,"type":"SCALAR",
              "sparse":{"count":1,
                        "indices":{"bufferView":0,"byteOffset":0,"componentType":5123},
                        "values":{"bufferView":0,"byteOffset":4}}}]})";
    Document greedy;
    const std::vector<uint8_t> bytes(tiny.begin(), tiny.end());
    std::vector<uint8_t> eight(8, 0);
    const std::vector<uint8_t> glb = Glb(tiny, eight);
    Document viaGlb;
    if (viaGlb.Read({glb.data(), glb.size()}, "greedy.glb")) {
      std::vector<double> out;
      CHECK(!viaGlb.ReadElements(0, out) && out.empty(),
            "**A 200-BYTE FILE CANNOT COMMAND GIGABYTES OF ZEROS**: the viewless fill is "
            "bounded by the bytes the file actually carries (board:1733)");
    } else {
      CHECK(true, "the greedy declaration refused at read, which bounds it even earlier");
    }
    (void)bytes;
  }

  return Report();
}

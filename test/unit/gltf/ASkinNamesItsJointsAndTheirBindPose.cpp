/* WHAT A SKIN IS BEFORE ANYTHING IS DEFORMED (board:1200). A joint list and one inverse bind matrix
 * per joint, paired by position -- so the reader's whole job is that the pairing survives and that a
 * file which cannot mean anything is refused by name rather than skinned into nonsense.
 *
 * THE ABSENT `inverseBindMatrices` IS THE CASE WORTH HAVING A TEST FOR. glTF states that when the
 * property is undefined every matrix is the identity, so an empty vector here is a MEANING and not a
 * gap; a reader that materialised N identities would make "the file declared them" and "the file
 * declared none" indistinguishable, and the round-trip could not tell them apart either. */
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"

using outshine::Gltf::Document;

namespace {

/* Two joints, and the inverse bind matrices are DISTINGUISHABLE from each other and from the
 * identity: joint 0 translates by (-1, 0, 0) and joint 1 by (0, -2, 0), column-major as the format
 * states, so a reader that transposed or that swapped the pair says so in the numbers. */
std::vector<uint8_t> Binary() {
  std::vector<uint8_t> out;
  const float first[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 1};
  const float second[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -2, 0, 1};
  for (float value : first) { outshine::Test::Append(out, value); }
  for (float value : second) { outshine::Test::Append(out, value); }
  return out;
}

const char *const kJson = R"({
  "asset": { "version": "2.0" },
  "scenes": [ { "nodes": [0] } ],
  "nodes": [
    { "name": "root", "children": [1, 3] },
    { "name": "joint", "children": [2] },
    { "name": "tip" },
    { "name": "skinned", "mesh": 0, "skin": 0 }
  ],
  "meshes": [ { "primitives": [ { "attributes": { "POSITION": 1 } } ] } ],
  "buffers": [ { "byteLength": 128 } ],
  "bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 128 } ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 2, "type": "MAT4" },
    { "bufferView": 0, "componentType": 5126, "count": 1, "type": "VEC3" }
  ],
  "skins": [ { "name": "arm", "skeleton": 1, "joints": [1, 2], "inverseBindMatrices": 0 } ]
})";

/* The same file with the matrices withdrawn, which the format defines as "every one is identity". */
const char *const kNoBind = R"({
  "asset": { "version": "2.0" },
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "children": [1] }, { "name": "joint" }, { "mesh": 0, "skin": 0 } ],
  "meshes": [ { "primitives": [ { "attributes": { "POSITION": 0 } } ] } ],
  "buffers": [ { "byteLength": 128 } ],
  "bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 128 } ],
  "accessors": [ { "bufferView": 0, "componentType": 5126, "count": 1, "type": "VEC3" } ],
  "skins": [ { "joints": [1] } ]
})";

bool Refuses(const char *json, const std::string &naming) {
  const std::vector<uint8_t> glb = outshine::Test::Glb(json, Binary());
  Document document;
  if (document.Read({glb.data(), glb.size()}, "skin.glb")) { return false; }
  return document.Error().find(naming) != std::string::npos;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> glb = Glb(kJson, Binary());
  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "skin.glb");
  CHECK(read, "a file carrying a skin is read");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }

  CHECK(document.Skins().size() == 1, "the file's one skin reaches the table");
  const auto &skin = document.Skins()[0];
  CHECK(skin.Name == "arm" && skin.Skeleton == 1, "the skin's name and skeleton root cross");
  CHECK(skin.Joints.size() == 2 && skin.Joints[0] == 1 && skin.Joints[1] == 2,
        "the joints cross in the file's own order, which is what pairs them with their matrices");
  CHECK(skin.InverseBind.size() == 32, "one 16-element matrix per joint and no more");
  CHECK(skin.InverseBind[12] == -1.0 && skin.InverseBind[13] == 0.0,
        "joint 0's inverse bind translates by (-1, 0, 0), read column-major as the format states");
  CHECK(skin.InverseBind[16 + 12] == 0.0 && skin.InverseBind[16 + 13] == -2.0,
        "joint 1's is the OTHER matrix, so the pairing is positional and was not transposed or swapped");
  CHECK(document.Nodes()[3].Skin == 0,
        "the skin reference sits on the node that instantiates the mesh, not on the mesh");
  CHECK(document.Nodes()[0].Skin == -1, "a node naming no skin carries -1 and not 0");

  const std::vector<uint8_t> bare = Glb(kNoBind, Binary());
  Document identity;
  const bool second = identity.Read({bare.data(), bare.size()}, "identity.glb");
  CHECK(second, "a skin that declares no inverseBindMatrices is read rather than refused");
  if (second) {
    CHECK(identity.Skins().size() == 1 && identity.Skins()[0].InverseBind.empty(),
          "an absent inverseBindMatrices stays EMPTY, so 'the format's identity' keeps a spelling "
          "distinct from a file that declared identities");
  }

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0,"skin":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
        "buffers":[{"byteLength":128}],"bufferViews":[{"buffer":0,"byteLength":128}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"VEC3"}],
        "skins":[{"joints":[]}]})",
                "names no joint"),
        "a skin with an empty joint list is refused by name");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0,"skin":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
        "buffers":[{"byteLength":128}],"bufferViews":[{"buffer":0,"byteLength":128}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"VEC3"}],
        "skins":[{"joints":[7]}]})",
                "which the file does not carry"),
        "a joint naming a node the file does not carry is refused by name");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0,"skin":3}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
        "buffers":[{"byteLength":128}],"bufferViews":[{"buffer":0,"byteLength":128}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"VEC3"}],
        "skins":[{"joints":[0]}]})",
                "and the file declares"),
        "a node naming a skin the file does not carry is refused by name");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"skin":0},{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
        "buffers":[{"byteLength":128}],"bufferViews":[{"buffer":0,"byteLength":128}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"VEC3"}],
        "skins":[{"joints":[1]}]})",
                "carries no mesh"),
        "a skin on a node with no mesh is refused, because glTF states a skin deforms the geometry "
        "the node instantiates and there is none");

  CHECK(Refuses(R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0,"skin":0},{"name":"j"}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":1}}]}],
        "buffers":[{"byteLength":128}],"bufferViews":[{"buffer":0,"byteLength":128}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"MAT4"},
                     {"bufferView":0,"componentType":5126,"count":1,"type":"VEC3"}],
        "skins":[{"joints":[1],"inverseBindMatrices":0}]})",
                "matrices"),
        "one joint against two bind matrices is refused, because the pairing is the record");

  Note("joints read", 2.0, "in one skin");
  Covers("I.26.6 skinning: skins, joints, inverse bind matrices and the node that names them");
  return Report();
}

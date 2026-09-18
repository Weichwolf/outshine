#include "Check.h"
#include "MeshImport.h"
#include "Document.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Gltf;
  using namespace outshine::Test;

  auto temporary = (std::filesystem::temp_directory_path() / "outshine-morph-XXXXXX").string();
  if (mkdtemp(temporary.data()) == nullptr) {
    Unprepared("fixture directory unavailable");
    return Report();
  }
  const std::filesystem::path root(temporary);
  {
    std::ofstream bytes(root / "data.bin", std::ios::binary);
    const std::array<float, 75> values = {
        0,  0,  0,  1,  0,  0,  0,  1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
        11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 31, 32,
        33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
        52, 53, 54, 55, 56, 57, 1,  0,  0,  1,  1,  0,  0,  1,  1,  0,  0,  1};
    bytes.write(reinterpret_cast<const char *>(values.data()), sizeof(values));
    CHECK(bytes.good(), "morph fixture written");
  }
  {
    std::ofstream file(root / "morph.gltf");
    file << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data.bin","byteLength":300}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":36},
        {"buffer":0,"byteOffset":108,"byteLength":36},{"buffer":0,"byteOffset":144,"byteLength":36},
        {"buffer":0,"byteOffset":180,"byteLength":36},{"buffer":0,"byteOffset":216,"byteLength":36},
        {"buffer":0,"byteOffset":252,"byteLength":48}],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":2,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":3,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":4,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":5,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":6,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":7,"componentType":5126,"count":3,"type":"VEC4"}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":0,"TANGENT":7},"targets":[
        {"POSITION":1,"NORMAL":2,"TANGENT":3},{"POSITION":4,"NORMAL":5,"TANGENT":6}]}]}],
      "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    CHECK(file.good(), "morph declaration written");
  }

  MeshAssetSet meshes;
  std::string error;
  {
    Document document;
    CHECK(document.ReadFile((root / "morph.gltf").string()), document.Error().c_str());
    CHECK(ImportMeshAssets(document, meshes, error), error.c_str());
  }
  const MeshPrimitive *primitive = meshes.Find(0, 0);
  CHECK(primitive != nullptr, "native morph primitive retained");
  if (primitive != nullptr) {
    CHECK(primitive->Positions == std::vector<float>({0, 0, 0, 1, 0, 0, 0, 1, 0}),
          "native base positions remain complete after the import document is gone");
    CHECK(primitive->MorphTargets.size() == 2, "morph target order retained");
    if (primitive->MorphTargets.size() == 2) {
      const MorphTargetDelta &first = primitive->MorphTargets[0];
      const MorphTargetDelta &second = primitive->MorphTargets[1];
      CHECK(first.Positions.front() == 1 && first.Normals.front() == 10 &&
                first.Tangents.front() == 19 && second.Positions.front() == 31 &&
                second.Normals.front() == 40 && second.Tangents.front() == 49 &&
                second.Tangents.back() == 57,
            "native morph deltas remain complete after the import document is gone");
    }
  }

  std::error_code cleanup;
  std::filesystem::remove_all(root, cleanup);
  CHECK(!cleanup, "fixture directory removed");
  return Report();
}

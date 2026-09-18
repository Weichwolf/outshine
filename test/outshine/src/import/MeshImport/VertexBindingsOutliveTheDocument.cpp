#include "Check.h"
#include "MeshImport.h"
#include "Document.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Gltf;
  using namespace outshine::Test;

  auto temporary = (std::filesystem::temp_directory_path() / "outshine-binding-XXXXXX").string();
  if (mkdtemp(temporary.data()) == nullptr) {
    Unprepared("fixture directory unavailable");
    return Report();
  }
  const std::filesystem::path root(temporary);
  {
    std::ofstream bytes(root / "data.bin", std::ios::binary);
    const std::array<float, 9> positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const std::array<uint8_t, 12> joints = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const std::array<float, 12> weights = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    const std::array<float, 16> bind = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    bytes.write(reinterpret_cast<const char *>(positions.data()), sizeof(positions));
    bytes.write(reinterpret_cast<const char *>(joints.data()), sizeof(joints));
    bytes.write(reinterpret_cast<const char *>(weights.data()), sizeof(weights));
    bytes.write(reinterpret_cast<const char *>(bind.data()), sizeof(bind));
    CHECK(bytes.good(), "vertex binding fixture written");
  }
  {
    std::ofstream file(root / "skin.gltf");
    file << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data.bin","byteLength":160}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":12},
        {"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":64}],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
        {"bufferView":1,"componentType":5121,"count":3,"type":"VEC4"},
        {"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},
        {"bufferView":3,"componentType":5126,"count":1,"type":"MAT4"}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2}}]}],
      "nodes":[{}, {"mesh":0,"skin":0}],"skins":[{"joints":[0],"inverseBindMatrices":3}],
      "scenes":[{"nodes":[0,1]}],"scene":0})";
    CHECK(file.good(), "vertex binding declaration written");
  }

  MeshAssetSet meshes;
  std::string error;
  {
    Document document;
    CHECK(document.ReadFile((root / "skin.gltf").string()), document.Error().c_str());
    CHECK(ImportMeshAssets(document, meshes, error), error.c_str());
  }
  const MeshPrimitive *primitive = meshes.Find(0, 0);
  CHECK(primitive != nullptr, "native mesh and primitive identity retained");
  if (primitive != nullptr) {
    CHECK(primitive->Skin.Sets == 1 && primitive->Skin.Vertices == 3 &&
              primitive->Skin.Joints == std::vector<uint32_t>(12, 0) &&
              primitive->Skin.Weights.size() == 12 && primitive->Skin.Weights[0] == 1 &&
              primitive->Skin.Weights[4] == 1 && primitive->Skin.Weights[8] == 1,
          "native vertex bindings remain complete after the import document is gone");
  }

  std::error_code cleanup;
  std::filesystem::remove_all(root, cleanup);
  CHECK(!cleanup, "fixture directory removed");
  return Report();
}

#include "Check.h"
#include "Document.h"
#include "SceneImport.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Gltf;
  using namespace outshine::Test;

  auto temporary = (std::filesystem::temp_directory_path() / "outshine-scene-XXXXXX").string();
  if (mkdtemp(temporary.data()) == nullptr) {
    Unprepared("fixture directory unavailable");
    return Report();
  }
  const std::filesystem::path root(temporary);
  {
    std::ofstream bytes(root / "data.bin", std::ios::binary);
    const std::array<float, 15> values = {0, 0, 0, 1, 0, 0, 0, 1, 0, 2, 3, 4, 5, 6, 7};
    bytes.write(reinterpret_cast<const char *>(values.data()), sizeof(values));
    CHECK(bytes.good(), "scene fixture written");
  }
  {
    std::ofstream file(root / "scene.gltf");
    file << R"({"asset":{"version":"2.0"},"extensionsUsed":["EXT_mesh_gpu_instancing"],
      "buffers":[{"uri":"data.bin","byteLength":60}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24}],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
        {"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
      "nodes":[{"mesh":0,"extensions":{"EXT_mesh_gpu_instancing":{"attributes":{"TRANSLATION":1}}}}],
      "scenes":[{"nodes":[0]}],"scene":0})";
    CHECK(file.good(), "scene declaration written");
  }

  SceneAsset scene;
  std::string error;
  {
    Document document;
    CHECK(document.ReadFile((root / "scene.gltf").string()), document.Error().c_str());
    CHECK(ImportSceneAsset(document, scene, error), error.c_str());
  }
  const SceneNodeAsset *node = scene.Node(0);
  CHECK(node != nullptr && node->Instances.size() == 2, "native instance count retained");
  if (node != nullptr && node->Instances.size() == 2) {
    Vec3 first;
    Vec3 second;
    node->Instances[0].Point({{0, 0, 0}}, first);
    node->Instances[1].Point({{0, 0, 0}}, second);
    CHECK(first == Vec3({{2, 3, 4}}) && second == Vec3({{5, 6, 7}}),
          "native instance transforms outlive the import document");
  }

  std::error_code cleanup;
  std::filesystem::remove_all(root, cleanup);
  CHECK(!cleanup, "fixture directory removed");
  return Report();
}

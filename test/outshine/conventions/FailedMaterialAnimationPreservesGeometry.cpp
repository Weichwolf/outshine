#include <import/GltfImporter.h>
#include "Check.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto pattern =
      (std::filesystem::temp_directory_path() / "outshine-material-failure-XXXXXX").string();
  if (mkdtemp(pattern.data()) == nullptr) {
    Unprepared("fixture directory unavailable");
    return Report();
  }

  struct Cleanup {
    std::filesystem::path Path;

    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(Path, ignored);
    }
  } cleanup{pattern};

  const auto &root = cleanup.Path;
  const std::array<float, 21> data{0, 0, 0, 1, 0, 0, 0, 1, 0,    0, 1,
                                   1, 0, 0, 1, 0, 0, 1, 1, 0.5f, 2};
  {
    std::ofstream file(root / "data.bin", std::ios::binary);
    file.write(reinterpret_cast<const char *>(data.data()), sizeof(data));
    CHECK(file.good(), "fixture bytes written");
  }
  {
    std::ofstream file(root / "scene.gltf");
    file << R"({"asset":{"version":"2.0"},"extensionsUsed":["KHR_animation_pointer"],
      "buffers":[{"uri":"data.bin","byteLength":84}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":8},
        {"buffer":0,"byteOffset":44,"byteLength":32},{"buffer":0,"byteOffset":76,"byteLength":8}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
        {"bufferView":1,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
        {"bufferView":2,"componentType":5126,"count":2,"type":"VEC4"},
        {"bufferView":3,"componentType":5126,"count":2,"type":"SCALAR"}],
      "materials":[{},{}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],
      "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0,
      "animations":[{"samplers":[{"input":1,"output":2},{"input":1,"output":3}],"channels":[
        {"sampler":0,"target":{"path":"pointer","extensions":{"KHR_animation_pointer":{"pointer":"/materials/0/pbrMetallicRoughness/baseColorFactor"}}}},
        {"sampler":1,"target":{"path":"pointer","extensions":{"KHR_animation_pointer":{"pointer":"/materials/1/pbrMetallicRoughness/roughnessFactor"}}}}]}]})";
    CHECK(file.good(), "fixture declaration written");
  }
  GltfImporter asset;
  const auto loaded = asset.load((root / "scene.gltf").string());
  CHECK(loaded.has_value(), "fixture loads at valid initial time");
  if (!loaded) { return Report(); }
  const std::array clips{0};
  CHECK(asset.selectAnimations(clips).has_value(), "valid initial sample selected");
  const Material first = asset.geometry().surfaceAt(MaterialInstance(0));
  const Material second = asset.geometry().surfaceAt(MaterialInstance(1));
  const float *positions = asset.geometry().positionsOf(0).data();
  CHECK(first.BaseColour[0] == 1 && second.Roughness == 0.5f,
        "initial sample matches independent values");
  CHECK(!asset.sampleAnimation(1), "later invalid factor rejects sample");
  CHECK(asset.geometry().surfaceAt(MaterialInstance(0)) == first &&
            asset.geometry().surfaceAt(MaterialInstance(1)) == second,
        "failed later factor preserves both previous materials");
  CHECK(asset.geometry().positionsOf(0).data() == positions,
        "failed sample retains borrowed geometry storage");
  CHECK(asset.sampleAnimation(0).has_value(), "valid retry succeeds");
  return Report();
}

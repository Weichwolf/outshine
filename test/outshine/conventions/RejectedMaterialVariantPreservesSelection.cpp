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
      (std::filesystem::temp_directory_path() / "outshine-variant-rollback-XXXXXX").string();
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
  const std::array<float, 15> data{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1};
  {
    std::ofstream file(root / "data.bin", std::ios::binary);
    file.write(reinterpret_cast<const char *>(data.data()), sizeof(data));
    CHECK(file.good(), "fixture geometry written");
  }
  {
    std::ofstream file(root / "scene.gltf");
    file << R"({"asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_variants"],
      "extensions":{"KHR_materials_variants":{"variants":[{"name":"broken"}]}},
      "buffers":[{"uri":"data.bin","byteLength":60}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}],
      "images":[{"uri":"missing.png"}],"textures":[{"source":0}],
      "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.25,0.5,0.75,1]}},
        {"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0,
        "extensions":{"KHR_materials_variants":{"mappings":[{"material":1,"variants":[0]}]}}}]}],
      "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    CHECK(file.good(), "fixture declaration written");
  }
  GltfImporter asset;
  const auto loaded = asset.load((root / "scene.gltf").string());
  CHECK(loaded.has_value(), "base material loads without unused variant texture");
  if (!loaded) { return Report(); }
  const auto material = asset.geometry().materialOf(0);
  const Material previous = asset.geometry().surfaceAt(material);
  const float *positions = asset.geometry().positionsOf(0).data();
  CHECK(!asset.selectMaterialVariant("broken"), "missing variant texture rejects conversion");
  CHECK(asset.geometry().materialOf(0) == material &&
            asset.geometry().surfaceAt(material) == previous &&
            asset.geometry().positionsOf(0).data() == positions,
        "failed selection preserves snapshot and views");
  CHECK(asset.sampleAnimation(0).has_value(), "later sample uses previous variant selection");
  CHECK(asset.geometry().surfaceAt(asset.geometry().materialOf(0)) == previous,
        "previous material remains selected");
  return Report();
}

#include <import/GltfImporter.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto pattern =
      (std::filesystem::temp_directory_path() / "outshine-material-owner-XXXXXX").string();
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

  const std::filesystem::path root = cleanup.Path;
  const std::array<float, 15> vertices{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1};
  const std::array<unsigned char, 68> png{
      {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
       0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x04, 0x00, 0x00,
       0x00, 0xb5, 0x1c, 0x0c, 0x02, 0x00, 0x00, 0x00, 0x0b, 0x49, 0x44, 0x41, 0x54, 0x78,
       0xda, 0x63, 0x64, 0xf8, 0x0f, 0x00, 0x01, 0x05, 0x01, 0x01, 0x27, 0x18, 0xe3, 0x66,
       0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82}};
  {
    std::ofstream file(root / "data.bin", std::ios::binary);
    file.write(reinterpret_cast<const char *>(vertices.data()), sizeof(vertices));
    CHECK(file.good(), "geometry fixture written");
  }
  {
    std::ofstream file(root / "pixel.png", std::ios::binary);
    file.write(reinterpret_cast<const char *>(png.data()),
               static_cast<std::streamsize>(png.size()));
    CHECK(file.good(), "image fixture written");
  }
  {
    std::ofstream file(root / "scene.gltf");
    file << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data.bin","byteLength":60}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}],
      "images":[{"uri":"pixel.png"}],"textures":[{"source":0}],
      "materials":[{"name":"paint","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0}]}],
      "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    CHECK(file.good(), "scene fixture written");
  }
  GltfImporter asset;
  const auto loaded = asset.load((root / "scene.gltf").string());
  CHECK(loaded.has_value(), loaded ? "textured asset loads" : loaded.error().c_str());
  if (!loaded) { return Report(); }
  CHECK(asset.geometry().surfaceNameOf(0) == "paint" && asset.geometry().images() == 1,
        "native material owns its name and decoded image");
  std::error_code removed;
  std::filesystem::remove(root / "pixel.png", removed);
  CHECK(!removed, "source image removed after import");
  const auto sampled = asset.sampleAnimation(0.0);
  CHECK(sampled.has_value(),
        sampled ? "native material rebuild ignores source files" : sampled.error().c_str());
  CHECK(asset.geometry().surfaceNameOf(0) == "paint" && asset.geometry().images() == 1,
        "rebuilt geometry retains the native material asset");
  return Report();
}

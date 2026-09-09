#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <scene/Loaded.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::string pattern = (std::filesystem::temp_directory_path() / "outshine-load-XXXXXX").string();
  const char *created = mkdtemp(pattern.data());
  CHECK(created != nullptr, "independent fixture directory exists");
  if (created == nullptr) { return Report(); }
  const std::filesystem::path directory(created);

  struct Cleanup {
    std::filesystem::path Directory;

    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(Directory, ignored);
    }
  } cleanup{directory};

  const auto write = [&](const char *name, const std::string &text) {
    std::ofstream file(directory / name, std::ios::binary);
    file << text;
    return file.good();
  };
  const std::string basic = R"({"asset":{"version":"2.0"},
    "buffers":[{"uri":"vertices.bin","byteLength":36}],
    "bufferViews":[{"buffer":0,"byteLength":36}],
    "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
      "min":[0,0,0],"max":[1,1,0]}],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  const std::array<float, 9> vertices{0, 0, 0, 1, 0, 0, 0, 1, 0};
  {
    std::ofstream buffer(directory / "vertices.bin", std::ios::binary);
    buffer.write(reinterpret_cast<const char *>(vertices.data()), sizeof(vertices));
    CHECK(buffer.good(), "fixture vertex buffer written");
  }
  std::string variant = basic;
  variant.insert(1, R"("extensionsUsed":["KHR_materials_variants"],
    "extensions":{"KHR_materials_variants":{"variants":[{"name":"alternate"}]}},)");
  CHECK(write("plain.gltf", basic) && write("variant.gltf", variant) && write("bad.gltf", "{"),
        "self-contained import fixtures written");
  std::string unusedCamera = basic;
  unusedCamera.insert(
      1, R"("cameras":[{"type":"perspective","perspective":{"yfov":1,"znear":0.1}}],)");
  CHECK(write("unused-camera.gltf", unusedCamera), "valid unused camera fixture written");
  Loaded asset;
  CHECK(asset.load((directory / "variant.gltf").string()).has_value(), "load variant asset");
  CHECK(asset.wears("alternate"), "select a valid variant before replacement");
  const int parts = asset.geometry().parts();
  CHECK(parts == 1, "fixture contains one mesh part");
  for (const char *name : {"missing.gltf", "bad.gltf"}) {
    const auto failed = asset.load((directory / name).string());
    CHECK(!failed && !failed.error().empty() && failed.error() == asset.error(),
          "failed import returns its own diagnostic");
    CHECK(asset.geometry().parts() == parts && asset.wears("alternate"),
          "failure preserves prior geometry, document and variant selection");
  }
  CHECK(asset.load((directory / "plain.gltf").string()).has_value(),
        "successful replacement resets a variant absent from the new asset");
  CHECK(asset.error().empty() && asset.geometry().parts() == 1,
        "replacement publishes native geometry and clears the old diagnostic");
  CHECK(asset.load((directory / "unused-camera.gltf").string()).has_value(),
        "uninstantiated camera definitions do not invalidate an asset");
  CHECK(asset.cameras() == 1 && !asset.carriesCamera(),
        "a camera definition without placement does not pretend to supply a view");
  Loaded moved = std::move(asset);
  CHECK(moved.geometry().parts() == 1, "move transfers the complete imported asset");
  CHECK(asset.load((directory / "plain.gltf").string()).has_value(),
        "load reinitializes a moved-from adapter");
  Covers("public loader replacement, rollback, selection reset and moved-from reuse using "
         "independent inputs");
  return Report();
}

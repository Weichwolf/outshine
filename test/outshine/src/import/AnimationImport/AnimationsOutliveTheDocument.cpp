#include "AnimationImport.h"
#include "Check.h"
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
  auto temporary =
      (std::filesystem::temp_directory_path() / "outshine-animation-assets-XXXXXX").string();
  if (mkdtemp(temporary.data()) == nullptr) {
    Unprepared("fixture directory unavailable");
    return Report();
  }
  const std::filesystem::path root(temporary);
  const std::array<float, 8> values{0, 1, 0, 0, 0, 2, 4, 6};
  {
    std::ofstream data(root / "data.bin", std::ios::binary);
    data.write(reinterpret_cast<const char *>(values.data()), sizeof(values));
    CHECK(data.good(), "animation samples written");
  }
  {
    std::ofstream file(root / "scene.gltf");
    file << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data.bin","byteLength":32}],
      "bufferViews":[{"buffer":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
        {"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}],
      "nodes":[{},{}],"scenes":[{"nodes":[0,1]}],"scene":0,
      "animations":[
        {"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
        {"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]},
        {"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}]})";
    CHECK(file.good(), "animation declaration written");
  }
  AnimationAssetSet animations;
  {
    Document document;
    CHECK(document.ReadFile((root / "scene.gltf").string()), document.Error().c_str());
    AnimationImport::ImportAll(document, animations);
  }
  CHECK(animations.Count() == 3, "all native animations outlive the import document");
  AnimationClip selected;
  std::string error;
  CHECK(animations.Select(std::array{0, 1}, selected, error),
        "disjoint native animations combine after document release");
  std::vector<AffineTransform> locals;
  std::vector<double> weights;
  selected.SamplePose(0.5, locals, weights);
  CHECK(locals.size() == 2 && locals[0].M[12] == 1 && locals[1].M[14] == 3,
        "combined native tracks retain their samples and rest pose");
  CHECK(!animations.Select(std::array{0, 2}, selected, error) && !error.empty(),
        "conflicting native tracks are rejected");
  selected.SamplePose(0.5, locals, weights);
  CHECK(locals.size() == 2 && locals[0].M[12] == 1 && locals[1].M[14] == 3,
        "failed native selection preserves the published clip");
  std::error_code cleanup;
  std::filesystem::remove_all(root, cleanup);
  CHECK(!cleanup, "fixture directory removed");
  return Report();
}

#include "Document.h"
#include "Pose.h"
#include "Check.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Gltf;
  using namespace outshine::Test;
  auto temporary = (std::filesystem::temp_directory_path() / "outshine-pose-XXXXXX").string();
  const auto *created = mkdtemp(temporary.data());
  CHECK(created != nullptr, "fixture directory created");
  if (!created) { return Report(); }
  const auto root = std::filesystem::path(temporary);
  const std::array<float, 8> values{0, 1, 0, 0, 0, 2, 4, 6};
  {
    std::ofstream data(root / "data.bin", std::ios::binary);
    data.write(reinterpret_cast<const char *>(values.data()), sizeof(values));
    CHECK(data.good(), "fixture samples written");
  }
  {
    std::ofstream file(root / "scene.gltf");
    file << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data.bin","byteLength":32}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
                   {"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}],
      "nodes":[{},{},{"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]}],"scenes":[{"nodes":[0,1,2]}],"scene":0,
      "animations":[
        {"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
        {"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]},
        {"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
        {"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":2,"path":"translation"}}]}
      ]})";
  }
  Document document;
  const bool loaded = document.ReadFile((root / "scene.gltf").string());
  CHECK(loaded, document.Error().c_str());
  if (!loaded) { return Report(); }
  Pose pose;
  std::string error;
  CHECK(Pose::Build(document, 0, pose, error), "initial pose built");
  const auto preserved = [&] {
    std::vector<Transform> locals;
    std::vector<double> weights;
    pose.At(0.5, locals, weights);
    return pose.Valid() && pose.NodeCount() == 3 && pose.ChannelCount() == 1 &&
           pose.StartS() == 0 && pose.EndS() == 1 && locals.size() == 3 && locals[0].M[12] == 1 &&
           locals[0].M[13] == 2 && locals[0].M[14] == 3 && locals[1].M[12] == 0;
  };
  CHECK(preserved(), "linear midpoint is analytically known");
  for (const auto &selection : {std::vector<int>{},
                                std::vector<int>{-1},
                                std::vector<int>{4},
                                std::vector<int>{1, 0, 2},
                                std::vector<int>{0, 0},
                                std::vector<int>{1, 3}}) {
    CHECK(Pose::Build(document, 0, pose, error) && preserved(),
          "each failure starts from a valid pose");
    CHECK(!Pose::Build(document, std::span<const int>(selection), pose, error) && !error.empty(),
          "invalid selection or late channel failure is diagnosed");
    CHECK(preserved(), "failed pose replacement preserves usable curves and metadata");
  }
  CHECK(Pose::Build(document, std::array{0, 1}, pose, error),
        "disjoint animation combination accepted");
  std::vector<Transform> locals;
  std::vector<double> weights;
  pose.At(0.5, locals, weights);
  CHECK(locals.size() == 3 && locals[0].M[12] == 1 && locals[1].M[14] == 3 &&
            pose.ChannelCount() == 2,
        "combined tracks retain their own stable sample buffers");
  CHECK(Pose::Build(document, 0, pose, error) && preserved() && error.empty(),
        "valid replacement remains usable");
  std::error_code cleanup;
  std::filesystem::remove_all(root, cleanup);
  CHECK(!cleanup, "fixture directory removed");
  return Report();
}

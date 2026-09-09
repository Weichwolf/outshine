#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <Outshine.h>
#include <scene/Loaded.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::string pattern =
      (std::filesystem::temp_directory_path() / "outshine-material-XXXXXX").string();
  if (mkdtemp(pattern.data()) == nullptr) {
    Unprepared("no fixture directory");
    return Report();
  }
  const std::filesystem::path root(pattern);

  struct Cleanup {
    std::filesystem::path Root;

    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(Root, ignored);
    }
  } cleanup{root};

  const std::array<float, 29> data{-1, -1, 0, 1,    -1, 0, 0, 1,    0, 0, 1, 1,    0,     0, 1,
                                   0,  0,  1, 0.5f, 0,  1, 1, 0.5f, 0, 0, 0, 0.5f, 0.25f, 1};
  {
    std::ofstream file(root / "data.bin", std::ios::binary);
    file.write(reinterpret_cast<const char *>(data.data()), sizeof(data));
    CHECK(file.good(), "independent animation and geometry bytes written");
  }
  const std::string json = R"({"asset":{"version":"2.0"},
    "extensionsUsed":["KHR_animation_pointer","KHR_materials_unlit","KHR_materials_emissive_strength"],
    "buffers":[{"uri":"data.bin","byteLength":116}],
    "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":8},
      {"buffer":0,"byteOffset":44,"byteLength":32},{"buffer":0,"byteOffset":76,"byteLength":8},
      {"buffer":0,"byteOffset":84,"byteLength":8},{"buffer":0,"byteOffset":92,"byteLength":24}],
    "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},
      {"bufferView":1,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
      {"bufferView":2,"componentType":5126,"count":2,"type":"VEC4"},
      {"bufferView":3,"componentType":5126,"count":2,"type":"SCALAR"},
      {"bufferView":4,"componentType":5126,"count":2,"type":"SCALAR"},
      {"bufferView":5,"componentType":5126,"count":2,"type":"VEC3"}],
    "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0,1,0,1]},"extensions":{"KHR_materials_unlit":{}}},
      {"pbrMetallicRoughness":{"metallicFactor":0.2,"roughnessFactor":0.7},"emissiveFactor":[0,0,0],
       "extensions":{"KHR_materials_emissive_strength":{"emissiveStrength":2}}}],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],
    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0,
    "animations":[{"samplers":[{"input":1,"output":2},{"input":1,"output":3},{"input":1,"output":4},{"input":1,"output":5}],
      "channels":[
      {"sampler":0,"target":{"path":"pointer","extensions":{"KHR_animation_pointer":{"pointer":"/materials/0/pbrMetallicRoughness/baseColorFactor"}}}},
      {"sampler":1,"target":{"path":"pointer","extensions":{"KHR_animation_pointer":{"pointer":"/materials/1/pbrMetallicRoughness/metallicFactor"}}}},
      {"sampler":2,"target":{"path":"pointer","extensions":{"KHR_animation_pointer":{"pointer":"/materials/1/pbrMetallicRoughness/roughnessFactor"}}}},
      {"sampler":3,"target":{"path":"pointer","extensions":{"KHR_animation_pointer":{"pointer":"/materials/1/emissiveFactor"}}}}]}]})";
  const auto write = [&](const std::string &text) {
    std::ofstream file(root / "scene.gltf");
    file << text;
    return file.good();
  };
  CHECK(write(json), "self-contained pointer fixture written");
  Loaded asset;
  CHECK(asset.load((root / "scene.gltf").string()).has_value(), "fixture imports");
  const std::array<int, 1> clips{0};
  CHECK(asset.plays(clips).has_value(), "material-only clip selects");
  for (const double time : {0.0, 1.0, 0.5, 0.25}) {
    CHECK(asset.poses(time).has_value(), "sample material-only animation at absolute time");
    const Material &colour = asset.geometry().surfaceAt(MaterialInstance(0));
    const Material &pbr = asset.geometry().surfaceAt(MaterialInstance(1));
    CHECK(std::abs(colour.BaseColour[0] - (1 - time)) < 1e-6 &&
              std::abs(colour.BaseColour[2] - time) < 1e-6 &&
              std::abs(colour.BaseColour[3] - (1 - 0.5 * time)) < 1e-6,
          "RGBA factors interpolate independently of node motion");
    CHECK(std::abs(pbr.Metalness - time) < 1e-6 &&
              std::abs(pbr.Roughness - (1 - 0.5 * time)) < 1e-6,
          "scalar metallic and roughness channels reach native material slots");
    CHECK(std::abs(pbr.Emission[0] - time) < 1e-6 &&
              std::abs(pbr.Emission[1] - 0.5 * time) < 1e-6 &&
              std::abs(pbr.Emission[2] - 2 * time) < 1e-6,
          "emission preserves strength even when authored emissive RGB is zero");
  }
  CHECK(asset.plays({}) && asset.geometry().surfaceAt(MaterialInstance(0)).BaseColour[1] == 1,
        "disabling clips restores authored values");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Engine engine;
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {320, 320};
  scene.Render.Outputs = {"sceneLinear"};
  Scenario::View view;
  view.Id = "material";
  view.Person = "first";
  view.Sees.Placed = true;
  view.Sees.Stands.AtM = {{0, 0, 3}};
  scene.Views.push_back(view);
  CHECK(asset.plays(clips).has_value(), "reselect material clip for rendering");
  if (!engine.drawsInto(scene.Render.Frame) || !engine.declare(scene)) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  for (const double time : {0.0, 1.0}) {
    CHECK(asset.poses(time).has_value(), "freeze an endpoint for native rendering");
    if (!engine.setGeometry(asset.geometry()) || !engine.assemble() || !engine.advance() ||
        !engine.renderer().render({})) {
      Unprepared(engine.error().c_str());
      return Report();
    }
    const auto path = std::filesystem::temp_directory_path() /
                      (time == 0 ? "outshine-material-red.png" : "outshine-material-blue.png");
    CHECK(engine.renderer().saveScreenshot(path.string()).has_value(),
          "capture material endpoint through public renderer");
  }
  for (const auto &change : std::array<std::pair<std::string, std::string>, 2>{
           {{"/materials/1/emissiveFactor", "/materials/9/emissiveFactor"},
            {"\"input\":1,\"output\":3", "\"input\":1,\"output\":2"}}}) {
    std::string bad = json;
    bad.replace(bad.find(change.first), change.first.size(), change.second);
    CHECK(write(bad), "malformed channel fixture written");
    Loaded invalid;
    CHECK(!invalid.load((root / "scene.gltf").string()) || !invalid.plays(clips),
          "invalid target or mismatched component width is refused");
  }
  return Report();
}

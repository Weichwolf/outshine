#include <array>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <import/GltfImporter.h>
#include <Outshine.h>
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
  GltfImporter asset;
  CHECK(asset.load((directory / "variant.gltf").string()).has_value(), "load variant asset");
  CHECK(asset.selectMaterialVariant("alternate").has_value(),
        "select a valid variant before replacement");
  const auto rejectedVariant = asset.selectMaterialVariant("absent");
  CHECK(!rejectedVariant && !rejectedVariant.error().empty(),
        "variant failure owns its diagnostic");
  CHECK(asset.selectMaterialVariant("alternate") && asset.error().empty(),
        "successful variant clears old error");
  CHECK(!rejectedVariant && !rejectedVariant.error().empty(),
        "returned error survives later successful mutation");
  const int parts = asset.geometry().parts();
  CHECK(parts == 1, "fixture contains one mesh part");
  for (const char *name : {"missing.gltf", "bad.gltf"}) {
    const auto failed = asset.load((directory / name).string());
    CHECK(!failed && !failed.error().empty() && failed.error() == asset.error(),
          "failed import returns its own diagnostic");
    CHECK(asset.geometry().parts() == parts && asset.selectMaterialVariant("alternate"),
          "failure preserves prior geometry, document and variant selection");
  }
  CHECK(asset.load((directory / "plain.gltf").string()).has_value(),
        "successful replacement resets a variant absent from the new asset");
  CHECK(asset.error().empty() && asset.geometry().parts() == 1,
        "replacement publishes native geometry and clears the old diagnostic");
  CHECK(asset.load((directory / "unused-camera.gltf").string()).has_value(),
        "uninstantiated camera definitions do not invalidate an asset");
  CHECK(asset.cameraCount() == 1 && !asset.hasDefaultCamera(),
        "a camera definition without placement does not pretend to supply a view");
  GltfImporter moved = std::move(asset);
  CHECK(moved.geometry().parts() == 1, "move transfers the complete imported asset");
  CHECK(asset.load((directory / "plain.gltf").string()).has_value(),
        "load reinitializes a moved-from adapter");
  const std::array<float, 8> animation{0, 1, 0, 0, 0, 4, 0, 0};
  {
    std::ofstream buffer(directory / "vertices.bin", std::ios::binary | std::ios::app);
    buffer.write(reinterpret_cast<const char *>(animation.data()), sizeof(animation));
    CHECK(buffer.good(), "independent linear animation keys written");
  }
  std::string animated = R"({"asset":{"version":"2.0"},
    "buffers":[{"uri":"vertices.bin","byteLength":68}],
    "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":8},
      {"buffer":0,"byteOffset":44,"byteLength":24}],
    "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
      "min":[0,0,0],"max":[1,1,0]},
      {"bufferView":1,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
      {"bufferView":2,"componentType":5126,"count":2,"type":"VEC3"}],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
    "cameras":[{"type":"perspective","perspective":{"yfov":1,"znear":0.1}}],
    "nodes":[{"children":[1,2],"rotation":[0,0,0.7071067811865476,0.7071067811865476]},
      {"camera":0,"translation":[1,0,2]},{"mesh":0}],
    "animations":[{"samplers":[{"input":1,"output":2,"interpolation":"LINEAR"}],
      "channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
    "scenes":[{"nodes":[0]}],"scene":0})";
  animated.insert(1, R"("extensionsUsed":["KHR_materials_unlit"],
    "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.8,0.3,1]},
      "extensions":{"KHR_materials_unlit":{}}}],)");
  animated.insert(animated.find("\"attributes\""), "\"material\":0,");
  CHECK(write("animated.gltf", animated), "parent-animated camera fixture written");
  CHECK(asset.load((directory / "animated.gltf").string()).has_value(),
        "load camera and mesh under a shared animated parent");
  const std::array<int, 1> clips{0};
  CHECK(asset.selectAnimations(clips).has_value(), "select the parent translation clip");
  for (const double seconds : {0.5, 1.0, 0.25, 2.0, 0.0}) {
    CHECK(asset.sampleAnimation(seconds).has_value(),
          "sample absolute time, including backward and beyond final key");
    Camera camera;
    CHECK(asset.hasDefaultCamera() && asset.camera(0, camera),
          "both camera accessors resolve current pose");
    const double x = 4 * std::min(seconds, 1.0);
    CHECK(std::abs(camera.PositionM[0] - x) < 1e-9 && std::abs(camera.PositionM[1] - 1) < 1e-9 &&
              std::abs(camera.PositionM[2] - 2) < 1e-9,
          "parent translation and quarter-turn rotate the child offset analytically");
    CHECK(std::abs(asset.camera().PositionM[0] - x) < 1e-9,
          "cached default camera uses the same time as explicit selection");
  }
  CHECK(asset.sampleAnimation(0.5).has_value(), "establish a nonzero pose for rejection checks");
  for (const double seconds :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    CHECK(!asset.sampleAnimation(seconds) && std::abs(asset.camera().PositionM[0] - 2) < 1e-9,
          "invalid time cannot mutate the accepted camera pose");
  }
  for (const int invalid : {-1, 1}) {
    const std::array<int, 1> rejected{invalid};
    CHECK(!asset.selectAnimations(rejected), "invalid clip selection fails");
    CHECK(std::abs(asset.durationS() - 1.0) < 1e-9,
          "rejected selection preserves the active animation duration");
    CHECK(asset.sampleAnimation(0.75) && std::abs(asset.camera().PositionM[0] - 3) < 1e-9,
          "previous animation remains sampleable after rejected selection");
  }
  const auto rejectedTime = asset.sampleAnimation(-1);
  CHECK(!rejectedTime && !rejectedTime.error().empty(), "time failure owns its diagnostic");
  CHECK(asset.sampleAnimation(0.5) && asset.error().empty(),
        "successful sampling clears old error");
  CHECK(!rejectedTime && !rejectedTime.error().empty(), "time diagnostic survives later sampling");
  CHECK(asset.selectAnimations({}) && std::abs(asset.camera().PositionM[0]) < 1e-9,
        "disabling clips restores authored camera transforms rather than stale sampled locals");
  CHECK(asset.selectAnimations(clips) && asset.sampleAnimation(0.5),
        "prepare the camera and geometry snapshot for rendering");
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
  view.Id = "sampled-camera";
  view.Person = "first";
  view.Sees = asset.camera();
  view.Placement = Scenario::CameraPlacement::Local;
  scene.Views.push_back(view);
  if (!engine.drawsInto(scene.Render.Frame) || !engine.declare(scene) ||
      !engine.setGeometry(asset.geometry()) || !engine.assemble() || !engine.advance() ||
      !engine.renderer().render({})) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  CHECK(engine.renderer()
            .saveScreenshot(
                (std::filesystem::temp_directory_path() / "outshine-animated-camera.png").string())
            .has_value(),
        "render the sampled native camera and geometry through the public API");
  Covers("public loader replacement, rollback, selection reset and moved-from reuse using "
         "independent inputs");
  return Report();
}

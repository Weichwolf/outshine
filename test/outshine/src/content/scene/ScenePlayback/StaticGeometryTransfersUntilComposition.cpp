#include "ScenePlayback.h"
#include "HeapProbe.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

Geometry Triangle(const char *name, float offset = 0.0F) {
  Geometry geometry;
  const auto part = geometry.addPart(name, MaterialInstance{});
  CHECK(part, "a native part is created");
  if (!part) { return geometry; }
  CHECK(geometry.setPositions(
            *part, std::array{offset, 0.0F, 0.0F, offset + 1, 0.0F, 0.0F, offset, 1.0F, 0.0F}),
        "a native triangle owns its positions");
  CHECK(geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "a native triangle owns its indices");
  return geometry;
}

void Handoff() {
  constexpr double kStaticSampleS = 3.5;
  Geometry geometry = Triangle("large static world");
  const std::vector<float> positions(size_t{3} * 262144, 0.0F);
  CHECK(geometry.setPositions(0, positions),
        "the static fixture has a multi-megabyte position stream");
  const float *const owned = geometry.positionsOf(0).data();
  ScenePlayback playback;
  const size_t before = HeapProbe::LiveBytes();
  playback.SetGeometry(std::move(geometry));
  const size_t after = HeapProbe::LiveBytes();
  CHECK(after <= before,
        "static ownership transfer allocates no second geometry or asset snapshot");
  CHECK(playback.HasGeometry() && !playback.IsLoaded() && !playback.IsAnimated(),
        "native static contents do not become an imported or animated asset");
  CHECK(playback.Snapshot().positionsOf(0).data() == owned,
        "the moved position storage remains the published snapshot");
  const auto revision = playback.Revision();
  std::string error;
  CHECK(playback.Sample(kStaticSampleS, error) && playback.TimeS() == kStaticSampleS,
        "a static snapshot still records the requested sample time");
  CHECK(playback.Revision() == revision && playback.Snapshot().positionsOf(0).data() == owned,
        "sampling static geometry changes neither revision nor storage");
  Geometry invalid = Triangle("invalid addition");
  CHECK(invalid.setTriangles(0, std::array<uint32_t, 3>{0, 1, 99}),
        "the rejected fixture defers its invalid index bound until composition");
  ScenePlayback rejected;
  rejected.SetGeometry(std::move(invalid));
  error.reserve(256);
  const size_t beforeFailure = HeapProbe::LiveBytes();
  CHECK(!playback.Append(std::move(rejected), error) && !error.empty(),
        "an invalid addition fails composition");
  const size_t afterFailure = HeapProbe::LiveBytes();
  CHECK(afterFailure <= beforeFailure, "failed composition retains no deferred static snapshot");
  CHECK(playback.Revision() == revision && playback.Snapshot().positionsOf(0).data() == owned,
        "failed composition preserves the published static owner");
  playback.Clear();
  CHECK(!playback.HasGeometry() && playback.Snapshot().parts() == 0 && playback.TimeS() == 0,
        "clear releases the static snapshot and resets playback state");
}

void Composition() {
  ScenePlayback first;
  ScenePlayback second;
  first.SetGeometry(Triangle("first", 2));
  second.SetGeometry(Triangle("second", 6));
  const auto revision = first.Revision();
  std::string error;
  CHECK(first.Append(std::move(second), error), "two native static owners compose");
  CHECK(first.Snapshot().parts() == 2 && first.Snapshot().positionsOf(0)[0] == 2 &&
            first.Snapshot().positionsOf(1)[0] == 6,
        "composition retains both native sources in order");
  CHECK(first.Revision() == revision + 1 && first.Sample(4.0, error) && first.TimeS() == 4.0,
        "composition publishes one revision and retains static sampling");
  ScenePlayback third;
  third.SetGeometry(Triangle("third", 10));
  CHECK(first.Append(std::move(third), error) && first.Snapshot().parts() == 3,
        "later composition retains previously materialized native sources");
}

double WorldX(const Geometry &geometry, int part) {
  const auto point = geometry.positionsOf(part);
  const auto &placement = geometry.placementOf(part);
  return placement[0] * point[0] + placement[4] * point[1] + placement[8] * point[2] +
         placement[12];
}

std::filesystem::path WriteAnimation() {
  const std::filesystem::path root(PlantedPath("static-playback-import"));
  std::filesystem::create_directories(root);
  {
    const std::array<float, 17> values{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 4, 0, 0};
    std::ofstream data(root / "data.bin", std::ios::binary);
    data.write(reinterpret_cast<const char *>(values.data()), sizeof(values));
    CHECK(data.good(), "native import fixture writes triangle and animation samples");
  }
  {
    std::ofstream file(root / "scene.gltf");
    file << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data.bin","byteLength":68}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":8},
        {"buffer":0,"byteOffset":44,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
        {"bufferView":1,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
        {"bufferView":2,"componentType":5126,"count":2,"type":"VEC3"}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
      "nodes":[{"name":"moving imported triangle","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0,
      "animations":[{"samplers":[{"input":1,"output":2,"interpolation":"LINEAR"}],
        "channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}]})";
    CHECK(file.good(), "native import fixture writes its scene declaration");
  }
  return root;
}

void ImportedAnimation() {
  constexpr float kGroundX = 20;
  constexpr float kRoadX = 30;
  constexpr double kLaterSampleS = 0.75;
  const auto root = WriteAnimation();
  ScenePlayback playback;
  playback.SetGeometry(Triangle("generated ground", kGroundX));
  const float *const owned = playback.Snapshot().positionsOf(0).data();
  const auto revision = playback.Revision();
  std::string error;
  CHECK(!playback.Load(
            {.Path = (root / "missing.gltf").string(), .Variant = {}}, true, {.Fps = 30}, error),
        "a failed import reports its error");
  CHECK(playback.Revision() == revision && !playback.IsLoaded() &&
            playback.Snapshot().positionsOf(0).data() == owned,
        "failed loading preserves the uncomposed native owner");
  CHECK(playback.Load(
            {.Path = (root / "scene.gltf").string(), .Variant = {}}, true, {.Fps = 30}, error),
        "an animated import can join a native static owner");
  CHECK(playback.Snapshot().parts() == 2 && playback.IsLoaded() && playback.IsAnimated(),
        "loading retains the generated ground and imported animated subject");
  CHECK(playback.Sample(0.5, error), "the composed animation samples");
  if (playback.Snapshot().parts() == 2) {
    CHECK(playback.Snapshot().positionsOf(0)[0] == kGroundX && WorldX(playback.Snapshot(), 1) == 2,
          "animation moves its imported part without moving generated ground");
  }
  ScenePlayback more;
  more.SetGeometry(Triangle("later static road", kRoadX));
  CHECK(playback.Append(std::move(more), error) && playback.Sample(kLaterSampleS, error),
        "a static owner can join an already animated composition");
  if (playback.Snapshot().parts() == 3) {
    CHECK(playback.Snapshot().positionsOf(0)[0] == kGroundX &&
              playback.Snapshot().positionsOf(2)[0] == kRoadX &&
              WorldX(playback.Snapshot(), 1) == 3,
          "later sampling retains both static owners and the imported pose");
  } else {
    CHECK(false, "all three parts remain after animated composition");
  }
  std::filesystem::remove_all(root);
}
}

int main() {
  Handoff();
  Composition();
  ImportedAnimation();
  return outshine::Test::Report();
}

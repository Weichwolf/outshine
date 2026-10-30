#include "OsmField.h"
#include "CppAllocationProbe.h"
#include "Json.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
constexpr size_t kMost = size_t{64} * 1024 * 1024;

struct Scenario {
  int Zoom = 0, X = 0, Y = 0;
  MvtSchema Schema = MvtSchema::Shortbread;
  std::vector<std::string> Layers;
  std::string Input, Digest;
};

std::optional<Scenario> ReadScenario(std::string_view path) {
  auto text = ReadTextFile(path, 65536);
  Json doc;
  if (!text || !doc.Parse(text->data(), text->size())) { return std::nullopt; }
  const auto root = doc.Root();
  const int schema = root["schema"].Int(-1);
  const auto layers = root["layers"];
  if (schema < 0 || schema > static_cast<int>(MvtSchema::OpenMapTiles) ||
      layers.GetKind() != Json::Kind::Array) {
    return std::nullopt;
  }
  Scenario result{.Zoom = root["zoom"].Int(-1),
                  .X = root["x"].Int(-1),
                  .Y = root["y"].Int(-1),
                  .Schema = static_cast<MvtSchema>(schema),
                  .Layers = {},
                  .Input = root["input"].Str(),
                  .Digest = root["sha256"].Str()};
  for (size_t at = 0; at < layers.Size(); ++at) {
    if (layers[at].GetKind() != Json::Kind::String) { return std::nullopt; }
    result.Layers.push_back(layers[at].Str());
  }
  return result;
}

int Measure(const Scenario &scenario) {
  const auto input = ReadTextFile(scenario.Input, kMost);
  if (!input || Sha256Hex(input->data(), input->size()) != scenario.Digest) {
    std::println(stderr, "missing or mismatched MVT input");
    return 2;
  }
  const auto bytes = std::span(reinterpret_cast<const uint8_t *>(input->data()), input->size());
  std::array<double, 7> elapsed{};
  Test::CppAllocationCounts allocated;
  size_t heap = 0;
  size_t features = 0;
  size_t points = 0;
  size_t nativeBytes = 0;
  std::string canonical;
  for (double &milliseconds : elapsed) {
    const auto started = std::chrono::steady_clock::now();
    Test::BeginCppAllocationProbe();
    const auto field = std::make_unique<OsmField>(scenario.Zoom, scenario.Layers, scenario.Schema);
    const auto accepted = field->Accept(scenario.X, scenario.Y, bytes);
    allocated = Test::EndCppAllocationProbe();
    milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    if (!accepted) {
      std::println(stderr, "MVT preparation rejected: {}", accepted.error());
      return 3;
    }
    const auto encoded = field->EncodeNativeTile(kMost);
    if (!encoded) { return 4; }
    const auto digest = Sha256Hex(encoded->data(), encoded->size());
    if (!canonical.empty() && digest != canonical) { return 5; }
    canonical = digest;
    nativeBytes = encoded->size();
    heap = field->HeapBytes();
    features = field->Features().size();
    points = field->Points().size() / 2;
  }
  std::ranges::sort(elapsed);
  std::println("{{\"input_bytes\":{},\"native_bytes\":{},\"native_sha256\":\"{}\","
               "\"model_capacity_bytes\":{},\"cpp_allocation_calls\":{},"
               "\"cpp_allocated_total_bytes\":{},\"features\":{},\"points\":{},"
               "\"samples\":7,\"median_prepare_ms\":{:.9g}}}",
               input->size(),
               nativeBytes,
               canonical,
               heap,
               allocated.Calls,
               allocated.Bytes,
               features,
               points,
               elapsed[3]);
  return 0;
}
}

int main(int argc, const char *const *argv) {
  if (argc != 2) {
    std::println(stderr, "usage: native_tile_prepare_cost scenario.json");
    return 1;
  }
  const auto scenario = ReadScenario(argv[1]);
  if (!scenario) { return 1; }
  return Measure(*scenario);
}

#include "SpeciesMixture.h"
#include "ReadTextFile.h"
#include "Json.h"
#include "Heap.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;

bool Sample(Json::Ref input) {
  constexpr size_t kSamplesMost = 1000000;
  const auto names = input["names"];
  const auto sourceWeights = input["weights"];
  const auto uniforms = input["uniforms"];
  if (names.GetKind() != Json::Kind::Array || sourceWeights.Size() != names.Size() ||
      uniforms.Size() == 0 || uniforms.Size() > kSamplesMost) {
    return false;
  }
  std::vector<std::string> owned;
  std::vector<double> weights;
  for (size_t at = 0; at < names.Size(); ++at) {
    owned.push_back(names[at].Str());
    weights.push_back(sourceWeights[at].Num());
  }
  std::vector<std::string_view> views;
  for (const auto &name : owned) { views.push_back(name); }
  const auto before = Heap::LiveBytes();
  auto mixture = Generators::SpeciesMixture::Prepare(views, weights);
  if (!mixture) { return false; }
  const auto preparedBytes = Heap::LiveBytes() - before;
  std::vector<double> draws;
  draws.reserve(uniforms.Size());
  for (size_t at = 0; at < uniforms.Size(); ++at) {
    const auto value = uniforms[at].Num(-1);
    if (!(value >= 0 && value < 1)) { return false; }
    draws.push_back(value);
  }
  std::vector<int32_t> selected(draws.size(), -1);
  const Heap::Tag sampling("species-mixture-choice");
  const auto takenBefore = Heap::TakenAt(sampling.Index());
  std::chrono::nanoseconds elapsed{};
  {
    const Heap::Tagged tag(sampling);
    const auto started = std::chrono::steady_clock::now();
    for (size_t at = 0; at < draws.size(); ++at) {
      const auto choice = mixture->Choose(draws[at]);
      if (choice) { selected[at] = static_cast<int32_t>(*choice); }
    }
    elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started);
  }
  const auto allocated = Heap::TakenAt(sampling.Index()) - takenBefore;
  std::print("{{\"sampling_ns\":{},\"prepared_live_bytes\":{},\"sampling_allocated_bytes\":{},"
             "\"heap_instrumented\":{},\"chosen\":[",
             elapsed.count(),
             preparedBytes,
             allocated,
             Heap::ProcessInstrumentationEnabled());
  for (size_t at = 0; at < selected.size(); ++at) {
    std::print("{}{}", at == 0 ? "" : ",", selected[at]);
  }
  std::print("]}}");
  return true;
}
}

int main(int argc, char **argv) {
  if (argc != 2) { return 2; }
  constexpr size_t kInputMostBytes = 32 * 1024 * 1024;
  const auto text = outshine::ReadTextFile(argv[1], kInputMostBytes);
  outshine::Json input;
  if (!text || !input.Parse(text->data(), text->size())) { return 2; }
  const auto cases = input.Root()["cases"];
  if (cases.GetKind() != outshine::Json::Kind::Array || cases.Size() > 128) { return 2; }
  std::print("{{\"cases\":[");
  for (size_t at = 0; at < cases.Size(); ++at) {
    if (at > 0) { std::print(","); }
    if (!Sample(cases[at])) { return 3; }
  }
  std::println("]}}");
  return 0;
}

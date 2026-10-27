#include "ShadowCasterSelection.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <print>
#include <ratio>
#include <span>
#include <vector>

namespace {
using namespace outshine::Render;
constexpr uint64_t kMaximumElements = 10000000;

struct Input {
  std::vector<DrawBatch> Batches;
  std::vector<uint32_t> Jobs;
  std::vector<GeometryClusterTemplate> Templates;
  std::vector<float> Spheres;
  std::vector<GpuPlacement> Rows;
};

template <class T, size_t Extent> bool Read(std::ifstream &file, std::span<T, Extent> into) {
  file.read(reinterpret_cast<char *>(into.data()), static_cast<std::streamsize>(into.size_bytes()));
  return static_cast<bool>(file);
}

bool Load(const char *path, Input &input) {
  std::ifstream file(path, std::ios::binary);
  std::array<uint64_t, 4> sizes{};
  if (!Read(file, std::span(sizes))) { return false; }
  for (const auto size : sizes) {
    if (size > kMaximumElements) { return false; }
  }
  input.Batches.resize(sizes[0]);
  input.Jobs.resize(sizes[1]);
  input.Spheres.resize(sizes[2]);
  input.Rows.resize(sizes[3]);
  for (auto &batch : input.Batches) {
    std::array<uint32_t, 8> fields{};
    BatchSphere bound{};
    if (!Read(file, std::span(fields)) || !Read(file, std::span(bound))) { return false; }
    batch.FirstIndex = fields[0];
    batch.IndexCount = fields[1];
    batch.ModelSlot = fields[2];
    batch.Instances = fields[3];
    batch.FirstJob = fields[4];
    batch.JobCount = fields[5];
    batch.Motion = fields[6] == 0 ? VertexMotion::Deforming : VertexMotion::Rigid;
    if (fields[7] != 0) { batch.WorldSphere = bound; }
  }
  if (!Read(file, std::span(input.Jobs)) || !Read(file, std::span(input.Spheres))) { return false; }
  for (size_t at = 0; at < input.Jobs.size(); at += 4) {
    input.Templates.push_back({.SphereIndex = input.Jobs[at],
                               .RelativeFirstIndex = input.Jobs[at + 2],
                               .IndexCount = input.Jobs[at + 3]});
  }
  for (auto &row : input.Rows) {
    if (!Read(file, std::span(row.Current))) { return false; }
  }
  return file.peek() == std::ifstream::traits_type::eof();
}
}

int main(int argc, char **argv) {
  if (argc != 2) { return 2; }
  Input input;
  if (!Load(argv[1], input)) { return 1; }
  ShadowCasterSelection selection;
  std::array<double, 10> milliseconds{};
  std::array<double, 10> preparationMs{};
  size_t qualified = 0;
  size_t ranges = 0;
  size_t sample = 0;
  for (auto &ms : milliseconds) {
    const auto began = std::chrono::steady_clock::now();
    selection.Prepare(input.Batches, input.Templates, input.Spheres, input.Rows, 0, true);
    preparationMs[sample++] =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    qualified = 0;
    ranges = 0;
    for (size_t at = 0; at < input.Batches.size(); ++at) {
      const auto span = selection.Ranges(at);
      qualified += span.empty() ? 0u : 1u;
      ranges += span.size();
    }
    ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  }
  const auto first = milliseconds[0];
  const auto prepareFirst = preparationMs[0];
  std::ranges::sort(std::span(milliseconds).subspan(1));
  std::ranges::sort(std::span(preparationMs).subspan(1));
  std::println(
      R"({{"batches":{},"qualified":{},"ranges":{},"heap_bytes":{},"first_ms":{},"warm_p50_ms":{},"warm_max_ms":{},"prepare_first_ms":{},"prepare_warm_p50_ms":{}}})",
      input.Batches.size(),
      qualified,
      ranges,
      selection.HeapBytes(),
      first,
      milliseconds[5],
      milliseconds[9],
      prepareFirst,
      preparationMs[5]);
  return 0;
}

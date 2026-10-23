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
  for (auto &ms : milliseconds) {
    const auto began = std::chrono::steady_clock::now();
    selection.Prepare(input.Batches, input.Jobs, input.Spheres, input.Rows, 0, true);
    ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  }
  const auto first = milliseconds[0];
  std::ranges::sort(std::span(milliseconds).subspan(1));
  size_t qualified = 0;
  size_t ranges = 0;
  for (size_t at = 0; at < input.Batches.size(); ++at) {
    const auto span = selection.Ranges(at);
    qualified += span.empty() ? 0u : 1u;
    ranges += span.size();
  }
  std::println(
      R"({{"batches":{},"qualified":{},"ranges":{},"heap_bytes":{},"first_ms":{},"warm_p50_ms":{},"warm_max_ms":{}}})",
      input.Batches.size(),
      qualified,
      ranges,
      selection.HeapBytes(),
      first,
      milliseconds[5],
      milliseconds[9]);
}

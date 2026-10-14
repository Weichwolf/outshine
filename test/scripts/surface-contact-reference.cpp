#include "scene/SurfaceSampler.h"

#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <print>
#include <ratio>
#include <vector>

namespace {
template <class T, size_t N> bool Read(std::array<T, N> &values) {
  return static_cast<bool>(std::cin.read(reinterpret_cast<char *>(values.data()),
                                         static_cast<std::streamsize>(N * sizeof(T))));
}

double Elapsed(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
      .count();
}
}

int main() {
  using namespace outshine;
  if (std::endian::native != std::endian::little || sizeof(double) != 8) { return 2; }
  std::ios_base::sync_with_stdio(false);
  const SurfaceSamplerLimits preparationLimits;
  const SurfaceQueryLimits queryLimits;
  std::array<uint64_t, 3> counts;
  if (!Read(counts) || counts[0] == 0 || counts[0] > preparationLimits.MaxVertices ||
      counts[1] == 0 || counts[1] > preparationLimits.MaxFaces || counts[2] == 0 ||
      counts[2] > queryLimits.MaxQueries) {
    return 2;
  }
  std::vector<Vec3> vertices(static_cast<size_t>(counts[0]));
  for (auto &vertex : vertices) {
    std::array<double, 3> point;
    if (!Read(point)) { return 2; }
    vertex = {{point[0], point[1], point[2]}};
  }
  std::vector<std::array<uint32_t, 3>> triangles(static_cast<size_t>(counts[1]));
  for (auto &triangle : triangles) {
    if (!Read(triangle)) { return 2; }
  }
  std::vector<Vec2> queries(static_cast<size_t>(counts[2]));
  for (auto &query : queries) {
    std::array<double, 2> point;
    if (!Read(point)) { return 2; }
    query = {{point[0], point[1]}};
  }
  const auto prepareStart = std::chrono::steady_clock::now();
  auto sampler = SurfaceSampler::prepare(vertices, triangles);
  const double prepareMs = Elapsed(prepareStart);
  if (!sampler) {
    std::println(stderr, "prepare error {}", static_cast<int>(sampler.error()));
    return 1;
  }
  const auto queryStart = std::chrono::steady_clock::now();
  auto samples = sampler->sample(queries);
  const double queryMs = Elapsed(queryStart);
  if (!samples) {
    std::println(stderr, "sample error {}", static_cast<int>(samples.error()));
    return 1;
  }
  std::println(stderr,
               R"({{"prepare_ms":{:.9g},"query_ms":{:.9g},"candidates":{},"batches":{}}})",
               prepareMs,
               queryMs,
               samples->Candidates,
               samples->Batches);
  for (const auto &contact : samples->Contacts) {
    std::println("{} {} {:.17g} {:.17g} {:.17g} {:.17g} {:.17g} {:.17g}",
                 contact.FaceIndex,
                 static_cast<int>(contact.Status),
                 contact.PositionM[0],
                 contact.PositionM[1],
                 contact.PositionM[2],
                 contact.Normal[0],
                 contact.Normal[1],
                 contact.Normal[2]);
  }
  return 0;
}

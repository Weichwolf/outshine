#include "PlantLimbPlan.h"
#include "PlantLimbAsset.h"
#include "PlantLimbSurface.h"
#include "PlantSurface.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <ratio>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
template <typename Value> bool Write(std::ofstream &output, std::span<const Value> values) {
  const auto bytes = std::as_bytes(values);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return output.good();
}

bool ReadLimb(std::vector<outshine::Generators::PlantLimbAnchor> &anchors,
              outshine::Generators::PlantLimbSurfaceDemand &demand) {
  size_t count = 0;
  if (!(std::cin >> count >> demand.Along >> demand.Sides) || count < 2 ||
      count > outshine::Generators::PlantLimbPlan::MostAnchors) {
    return false;
  }
  anchors.resize(count);
  for (auto &anchor : anchors) {
    if (!(std::cin >> anchor.Node >> anchor.PositionM[0] >> anchor.PositionM[1] >>
          anchor.PositionM[2] >> anchor.RadiusM)) {
      return false;
    }
  }
  return true;
}

bool Restore(outshine::Generators::PlantLimbPlan &plan) {
  using namespace outshine::Generators;
  constexpr size_t kBudget = 65536;
  const auto bytes = EncodePlantLimbs(std::span(&plan, 1), kBudget);
  if (!bytes) { return false; }
  auto decoded = DecodePlantLimbs(*bytes, kBudget);
  if (!decoded || decoded->size() != 1) { return false; }
  plan = std::move(decoded->front());
  return true;
}

bool WriteSamples(std::ofstream &output,
                  const outshine::Generators::PlantLimbPlan &plan,
                  size_t stations) {
  for (size_t at = 0; at < stations; ++at) {
    const double parameter =
        at + 1 == stations ? 1
                           : static_cast<double>(at) * (1.0 / static_cast<double>(stations - 1));
    const auto sample = plan.Sample(parameter);
    if (!sample) { return false; }
    const std::array row{sample->CentreM[0],
                         sample->CentreM[1],
                         sample->CentreM[2],
                         sample->Tangent[0],
                         sample->Tangent[1],
                         sample->Tangent[2],
                         sample->Curvature[0],
                         sample->Curvature[1],
                         sample->Curvature[2],
                         sample->SpeedM,
                         sample->RadiusM,
                         sample->RadiusSlopeM};
    if (!Write(output, std::span<const double>(row))) { return false; }
  }
  return true;
}

int Run(std::ofstream &output, size_t cases, bool roundtrip) {
  using namespace outshine::Generators;
  size_t vertices = 0;
  size_t triangles = 0;
  double preparationMs = 0;
  double surfaceMs = 0;
  double roundtripMs = 0;
  for (size_t item = 0; item < cases; ++item) {
    PlantLimbSurfaceDemand demand;
    std::vector<PlantLimbAnchor> anchors;
    if (!ReadLimb(anchors, demand)) { return 2; }
    const auto began = std::chrono::steady_clock::now();
    auto plan = PlantLimbPlan::Create(anchors);
    const auto prepared = std::chrono::steady_clock::now();
    if (!plan) {
      std::cerr << item << ": " << plan.error() << '\n';
      return 3;
    }
    if (roundtrip && !Restore(*plan)) { return 7; }
    const auto restored = std::chrono::steady_clock::now();
    demand.MaximumVertices = 65536;
    demand.MaximumTriangles = 262144;
    const auto mesh = BuildPlantLimbSurface(*plan, demand);
    const auto built = std::chrono::steady_clock::now();
    if (!mesh) {
      std::cerr << item << ": " << mesh.error() << '\n';
      return 4;
    }
    preparationMs += std::chrono::duration<double, std::milli>(prepared - began).count();
    surfaceMs += std::chrono::duration<double, std::milli>(built - restored).count();
    if (roundtrip) {
      roundtripMs += std::chrono::duration<double, std::milli>(restored - prepared).count();
    }
    const size_t stations = (anchors.size() - 1) * demand.Along + 1;
    const std::array<uint64_t, 3> header{
        stations, mesh->Positions.size() / 3, mesh->Triangles.size() / 3};
    if (!Write(output, std::span<const uint64_t>(header))) { return 5; }
    if (!WriteSamples(output, *plan, stations)) { return 6; }
    if (!Write(output, std::span<const float>(mesh->Positions)) ||
        !Write(output, std::span<const float>(mesh->Normals)) ||
        !Write(output, std::span<const uint32_t>(mesh->Triangles))) {
      return 5;
    }
    vertices += mesh->Positions.size() / 3;
    triangles += mesh->Triangles.size() / 3;
  }
  std::cout << cases << ' ' << vertices << ' ' << triangles << ' ' << preparationMs << ' '
            << surfaceMs << ' ' << roundtripMs << '\n';
  return 0;
}
}

int main(int argc, const char *const *argv) {
  if (argc < 2 || argc > 3 || (argc == 3 && std::string_view(argv[2]) != "--roundtrip")) {
    return 1;
  }
  std::ofstream output(argv[1], std::ios::binary);
  size_t cases = 0;
  if (!output || !(std::cin >> cases) || cases > 65536) { return 1; }
  const int result = Run(output, cases, argc == 3);
  output.close();
  return output.fail() ? 5 : result;
}

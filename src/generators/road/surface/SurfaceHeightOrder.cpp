#include "SurfaceHeightProgramInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace outshine::Generators::HeightAssembly {
namespace {
bool OrderCorners(
    const Input &input, PlanarFace face, PlanarFace other, int order, System &system) {
  if (order < 0) { return system.Fail(Error::InvalidInput); }
  for (size_t corner = 0; corner < face.size(); ++corner) {
    if (input.Points[face[corner]] != input.Points[other[corner]]) {
      return system.Fail(Error::InvalidInput);
    }
    const std::array columns = order == 1 ? std::array{face[corner], other[corner]}
                                          : std::array{other[corner], face[corner]};
    const std::array values{1., -1.};
    system.Add(columns, values, 0);
    ++system.Work.OverlapRows;
  }
  return system.Check();
}

bool OrderPair(const Input &input,
               const std::map<uint32_t, PlanarFace> &first,
               const std::map<uint32_t, PlanarFace> &second,
               int order,
               System &system) {
  for (const auto &[id, face] : first) {
    if (!system.Check()) { return false; }
    const auto match = second.find(id);
    if (match == second.end() || match->second == face) { continue; }
    if (!OrderCorners(input, face, match->second, order, system)) { return false; }
  }
  return true;
}
}

bool Overlaps(const Input &input, System &system) {
  std::vector<std::map<uint32_t, PlanarFace>> mapped(input.Sources.size());
  for (size_t owner = 0; owner < input.Sources.size(); ++owner) {
    const auto &source = input.Sources[owner];
    for (size_t at = 0; at < source.Faces.size(); ++at) {
      auto face = source.Faces[at];
      std::ranges::sort(face,
                        [&](uint32_t a, uint32_t b) { return input.Points[a] < input.Points[b]; });
      if (!mapped[owner].emplace(source.FaceIds[at], face).second) {
        return system.Fail(Error::InvalidInput);
      }
    }
  }
  for (size_t a = 0; a < input.Sources.size(); ++a) {
    for (size_t b = a + 1; b < input.Sources.size(); ++b) {
      if (!OrderPair(
              input, mapped[a], mapped[b], input.Orders[a * input.Sources.size() + b], system)) {
        return false;
      }
    }
  }
  return true;
}
}

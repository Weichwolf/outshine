#include "TerrainLattice.h"
#include "TerrainSurfacePublication.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <utility>

namespace outshine {
static_assert(kPatchGrid + 1 == Render::TerrainLattice::kSide,
              "native height pages and the renderer share one lattice");

bool TerrainSurfacePublication::StitchSeams(Patchwork &laid, std::string &error) {
  if (!Framed_) { return true; }
  auto stitched = Generators::Terrain::StitchTerrainSeams(laid);
  if (!stitched) {
    Seams_ = {};
    error = std::move(stitched.error());
    return false;
  }
  Seams_ = *stitched;
  return true;
}

bool TerrainSurfacePublication::BeginResidency(const Patchwork &laid, std::string &error) {
  return !Framed_ || Residency_.BeginPublish(laid, error);
}

std::expected<bool, std::string> TerrainSurfacePublication::AdvanceResidency(const Patchwork &laid,
                                                                             size_t sheetsMost) {
  if (!Framed_) { return true; }
  return Residency_.AdvancePublish(laid, Frame_, sheetsMost);
}

void TerrainSurfacePublication::Clear() {
  Residency_.Clear();
}

uint64_t TerrainSurfacePublication::Digest() const {
  return Residency_.Digest();
}

size_t TerrainSurfacePublication::HeapBytes(TerrainHeapScope scope) const noexcept {
  return Residency_.HeapBytes(scope);
}
}

#include "HeightSheets.h"
#include "TerrainRenderer.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace outshine {
static_assert(kPatchGrid + 1 == Render::TerrainRenderer::kSide,
              "native height pages and the renderer share one lattice");

bool HeightSheets::Stitch(Patchwork &laid, std::string &error) {
  if (!Framed_) { return true; }
  return StitchEdges(laid, error);
}

bool HeightSheets::Hands(Patchwork &laid, std::string &error) {
  return Stitch(laid, error) && (!Framed_ || Residency_.Publish(laid, Frame_, error));
}

bool HeightSheets::BeginResidency(const Patchwork &laid, std::string &error) {
  return !Framed_ || Residency_.BeginPublish(laid, error);
}

std::expected<bool, std::string> HeightSheets::AdvanceResidency(const Patchwork &laid,
                                                                size_t sheetsMost) {
  if (!Framed_) { return true; }
  return Residency_.AdvancePublish(laid, Frame_, sheetsMost);
}

void HeightSheets::Clear() {
  Residency_.Clear();
}

uint64_t HeightSheets::Digest() const {
  return Residency_.Digest();
}

size_t HeightSheets::HeapBytes(TerrainHeapScope scope) const noexcept {
  return Residency_.HeapBytes(scope);
}
}

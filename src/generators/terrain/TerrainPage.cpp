#include "TerrainPage.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "ChunkSurface.h"

namespace outshine::Generators {

bool TerrainPageLayout::Valid() const noexcept {
  if (Side < 2 || Halo < 0) { return false; }
  const int64_t pageSide = static_cast<int64_t>(Side) + 2 * static_cast<int64_t>(Halo);
  if (pageSide > static_cast<int64_t>(std::numeric_limits<int>::max())) { return false; }
  const auto widened = static_cast<size_t>(pageSide);
  return widened <= std::numeric_limits<size_t>::max() / widened;
}

size_t TerrainPageLayout::PageSide() const noexcept {
  return static_cast<size_t>(Side) + 2u * static_cast<size_t>(Halo);
}

size_t TerrainPageLayout::NodeCount() const noexcept {
  return PageSide() * PageSide();
}

size_t TerrainPageLayout::NodeAt(int column, int row) const noexcept {
  return static_cast<size_t>(row + Halo) * PageSide() + static_cast<size_t>(column + Halo);
}

double TerrainPageLayout::FractionAt(const Sheet &sheet, int node) const noexcept {
  if (sheet.Virtual || sheet.SourceZoom >= 0) {
    return static_cast<double>(node) / static_cast<double>(Side - 1);
  }
  const auto posting = [&](int at) {
    return static_cast<double>(Ground::ChunkNodePosting(at, sheet.Postings, Side)) /
           static_cast<double>(sheet.Postings - 1u);
  };
  if (node < 0) { return -posting(1); }
  if (node >= Side) { return 2.0 - posting(Side - 2); }
  return posting(node);
}

}

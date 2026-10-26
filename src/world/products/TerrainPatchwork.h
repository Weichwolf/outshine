#ifndef OUTSHINE_WORLD_PRODUCTS_TERRAINPATCHWORK_H
#define OUTSHINE_WORLD_PRODUCTS_TERRAINPATCHWORK_H

#include "TileId.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace outshine {

constexpr size_t kZoomLevels = 24;

struct Sheet {
  Data::TileId Tile;
  std::vector<float> Nodes;
  int Side = 0;
  uint32_t Postings = 0;
  bool Virtual = false;
  int SourceZoom = -1;

  [[nodiscard]] bool operator==(const Sheet &) const = default;
};

struct Patchwork {
  std::vector<Sheet> Sheets;
  size_t Tiles = 0;
  size_t Pending = 0;
  size_t Absent = 0;
  size_t Refused = 0;
  size_t Skipped = 0;
  size_t Bare = 0;
  size_t ContactPending = 0;
  std::array<int, kZoomLevels> PendingAtZoom = {{}};
  std::array<int, kZoomLevels> WantedAtZoom = {{}};
  size_t Overlapped = 0;
  long ReachTiles = 0;
  int CoarsestZoom = 0;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = Sheets.capacity() * sizeof(Sheet);
    for (const Sheet &sheet : Sheets) { bytes += sheet.Nodes.capacity() * sizeof(float); }
    return bytes;
  }
};

}
#endif

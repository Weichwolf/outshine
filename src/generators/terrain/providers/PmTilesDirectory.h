#ifndef OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_PMTILESDIRECTORY_H
#define OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_PMTILESDIRECTORY_H

#include <Address.h>
#include <SourceTransport.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators::Terrain {

class PmTilesDirectory {
public:
  static constexpr size_t RootWindowBytes = size_t{16} * 1024;
  static constexpr size_t DirectoryBytesMost = size_t{256} * 1024;

  struct Layout {
    Data::ByteRange Root, Leaves, Tiles;
    int MinimumZoom = 0, MaximumZoom = 0;
    bool Gzip = false;
  };

  struct Entry {
    uint64_t TileId = 0;
    uint64_t Run = 0;
    Data::ByteRange Bytes;
  };

  [[nodiscard]] static std::optional<Layout> ReadLayout(std::span<const uint8_t> prefix,
                                                        uint64_t archiveBytes);

  [[nodiscard]] static std::optional<PmTilesDirectory> Read(std::span<const uint8_t> bytes,
                                                            const Layout &layout);

  [[nodiscard]] static uint64_t TileId(Data::TileId tile) noexcept;
  [[nodiscard]] const Entry *Find(uint64_t id) const noexcept;

private:
  std::vector<Entry> Entries_;
};

}
#endif

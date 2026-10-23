#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANAREAGROUPS_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANAREAGROUPS_H

#include "UrbanArea.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>

namespace outshine::Generators::Osm {
class UrbanAreaGroups {
public:
  struct Layout {
    uint32_t PatchesPerSide = 1;
    size_t GroupsMost = 0;
  };

  struct Selection {
    size_t Index = 0;
    size_t PatchesMost = 0;
  };

  [[nodiscard]] static std::expected<UrbanAreaGroups, std::string_view> Make(const UrbanArea &area,
                                                                             Layout layout);
  [[nodiscard]] size_t Size() const noexcept;
  [[nodiscard]] std::optional<UrbanPatchRange> At(size_t index) const noexcept;
  [[nodiscard]] std::expected<std::vector<UrbanPatchRange>, std::string_view>
  Patches(Selection selection) const;

private:
  uint32_t Columns_ = 0, Rows_ = 0, PatchSide_ = 0, GroupSide_ = 0, Across_ = 0, Down_ = 0;
};
}
#endif

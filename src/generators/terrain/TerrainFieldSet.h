#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINFIELDSET_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINFIELDSET_H

#include <cstddef>
#include <cstdint>
#include <array>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "HeightField.h"
#include "TerrainGrid.h"

namespace outshine::Generators {

class TerrainFieldSet {
public:
  using Entry = std::pair<Data::TileId, std::shared_ptr<const ::outshine::Ground::TerrainField>>;

  enum class BindingError { InvalidRequest, OverBudget };
  enum class RangeError { InvalidRequest, MissingSource, IncompleteSource, OverBudget };

  struct HeightRange {
    std::array<double, 2> AslM;
    size_t Nodes = 0;
  };

  struct Binding {
    std::array<uint8_t, 32> Key;
    size_t Fields = 0;
    size_t RasterBytes = 0;
    size_t MetadataBytes = 0;
    size_t CopiedRasterBytes = 0;
  };

  [[nodiscard]] std::expected<Binding, BindingError> BindingAt(int zoom) const;

  [[nodiscard]] bool ShareSourcedField(Data::TileId tile,
                                       ::outshine::Ground::HeightField::Block &into) const;

  [[nodiscard]] static bool Share(std::span<const Entry> fields,
                                  Data::TileId tile,
                                  ::outshine::Ground::HeightField::Block &into);

  TerrainFieldSet() = default;

  explicit TerrainFieldSet(std::vector<Entry> fields,
                           ::outshine::Ground::TerrainRevisionIndex::Reservation metadata = nullptr)
      : Fields_(std::move(fields)), Metadata_(std::move(metadata)) {}

  [[nodiscard]] bool CopySourcedField(Data::TileId tile,
                                      ::outshine::Ground::HeightField::Block &into) const;
  [[nodiscard]] static bool Copy(std::span<const Entry> fields,
                                 Data::TileId tile,
                                 ::outshine::Ground::HeightField::Block &into);
  [[nodiscard]] std::optional<double> AslMAt(int zoom, LongitudeLatitude at) const;
  [[nodiscard]] static std::optional<double>
  AslMAt(std::span<const Entry> fields, int zoom, LongitudeLatitude at);
  [[nodiscard]] std::expected<HeightRange, RangeError>
  AslRangeOver(int zoom, ::outshine::Ground::GeoBounds bounds, size_t nodesMost) const;

private:
  std::vector<Entry> Fields_;
  ::outshine::Ground::TerrainRevisionIndex::Reservation Metadata_;
};

}

#endif

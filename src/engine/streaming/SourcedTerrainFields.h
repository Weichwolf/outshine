#ifndef OUTSHINE_ENGINE_STREAMING_SOURCEDTERRAINFIELDS_H
#define OUTSHINE_ENGINE_STREAMING_SOURCEDTERRAINFIELDS_H

#include <cstddef>
#include <array>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "HeightField.h"
#include "TerrainGrid.h"

namespace outshine {

class SourcedTerrainFields {
public:
  using Entry = std::pair<Data::TileId, std::shared_ptr<const Ground::TerrainField>>;

  enum class CaptureError { InvalidRequest, MissingSource, OverBudget };
  enum class RangeError { InvalidRequest, MissingSource, IncompleteSource, OverBudget };

  struct HeightRange {
    std::array<double, 2> AslM;
    size_t Nodes = 0;
  };

  [[nodiscard]] static std::expected<SourcedTerrainFields, CaptureError>
  Capture(std::span<const Entry> fields,
          std::span<const Ground::TileSpot> requests,
          size_t bytesMost,
          Ground::TerrainRevisionIndex::Reservation metadata = nullptr);
  [[nodiscard]] size_t RetainedBytes() const noexcept;
  [[nodiscard]] size_t PreparationBytes() const noexcept;
  [[nodiscard]] bool FitsPreparation(std::span<const Ground::TileSpot> requests,
                                     size_t bytesMost) const;
  [[nodiscard]] bool ShareSourcedField(Data::TileId tile, Ground::HeightField::Block &into) const;

  [[nodiscard]] static bool
  Share(std::span<const Entry> fields, Data::TileId tile, Ground::HeightField::Block &into);

  SourcedTerrainFields() = default;

  explicit SourcedTerrainFields(std::vector<Entry> fields,
                                Ground::TerrainRevisionIndex::Reservation metadata = nullptr)
      : Fields_(std::move(fields)), Metadata_(std::move(metadata)) {}

  [[nodiscard]] bool CopySourcedField(Data::TileId tile, Ground::HeightField::Block &into) const;
  [[nodiscard]] static bool
  Copy(std::span<const Entry> fields, Data::TileId tile, Ground::HeightField::Block &into);
  [[nodiscard]] std::optional<double> AslMAt(int zoom, LongitudeLatitude at) const;
  [[nodiscard]] static std::optional<double>
  AslMAt(std::span<const Entry> fields, int zoom, LongitudeLatitude at);
  [[nodiscard]] std::expected<HeightRange, RangeError>
  AslRangeOver(int zoom, Ground::GeoBounds bounds, size_t nodesMost) const;

private:
  std::vector<Entry> Fields_;
  Ground::TerrainRevisionIndex::Reservation Metadata_;
};

}

#endif

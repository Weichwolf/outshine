#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINFIELDSET_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINFIELDSET_H

#include <cstddef>
#include <cstdint>
#include <array>
#include <cmath>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
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

  [[nodiscard]] std::expected<Binding, BindingError>
  BilinearBindingAt(int zoom, std::string_view sourceDatum, std::string_view heightMapping) const;

  [[nodiscard]] bool ShareOriginalField(Data::TileId tile,
                                        ::outshine::Ground::HeightField::Block &into) const;

  [[nodiscard]] bool ShareSourcedField(Data::TileId tile,
                                       ::outshine::Ground::HeightField::Block &into) const;

  [[nodiscard]] static bool Share(std::span<const Entry> fields,
                                  Data::TileId tile,
                                  ::outshine::Ground::HeightField::Block &into);

  TerrainFieldSet() = default;

  explicit TerrainFieldSet(
      std::vector<Entry> fields,
      ::outshine::Ground::TerrainRevisionIndex::Reservation metadata = nullptr);

  [[nodiscard]] bool CopySourcedField(Data::TileId tile,
                                      ::outshine::Ground::HeightField::Block &into) const;
  [[nodiscard]] static bool Copy(std::span<const Entry> fields,
                                 Data::TileId tile,
                                 ::outshine::Ground::HeightField::Block &into);
  [[nodiscard]] std::optional<double> AslMAt(int zoom, LongitudeLatitude at) const;

  template <class Lookup>
  [[nodiscard]] static std::optional<double>
  AslMAt(int zoom, LongitudeLatitude at, const Lookup &lookup) {
    for (int heldZoom = zoom; heldZoom >= 0; --heldZoom) {
      const auto frac = ::outshine::Ground::ToTileFracClamped(
          {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg}, heldZoom);
      const Data::TileId tile{.Zoom = heldZoom,
                              .X = static_cast<uint32_t>(std::floor(frac.X)),
                              .Y = static_cast<uint32_t>(std::floor(frac.Y))};
      const auto *field = lookup(tile);
      if (field == nullptr || !field->Meshable()) { continue; }
      return field->PostingM(
          {.Col = frac.X - std::floor(frac.X), .Row = frac.Y - std::floor(frac.Y)});
    }
    return std::nullopt;
  }

  [[nodiscard]] std::expected<HeightRange, RangeError>
  AslRangeOver(int zoom, ::outshine::Ground::GeoBounds bounds, size_t nodesMost) const;

private:
  [[nodiscard]] std::expected<Binding, BindingError> BindAt(int zoom,
                                                            std::string_view sampling,
                                                            std::string_view sourceDatum,
                                                            std::string_view heightMapping) const;

  std::vector<Entry> Fields_;
  ::outshine::Ground::TerrainRevisionIndex::Reservation Metadata_;
};

}

#endif

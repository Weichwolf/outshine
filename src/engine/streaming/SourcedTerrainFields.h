#ifndef OUTSHINE_ENGINE_STREAMING_SOURCEDTERRAINFIELDS_H
#define OUTSHINE_ENGINE_STREAMING_SOURCEDTERRAINFIELDS_H

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

  SourcedTerrainFields() = default;

  explicit SourcedTerrainFields(std::vector<Entry> fields) : Fields_(std::move(fields)) {}

  [[nodiscard]] bool CopySourcedField(Data::TileId tile, Ground::HeightField::Block &into) const;
  [[nodiscard]] static bool
  Copy(std::span<const Entry> fields, Data::TileId tile, Ground::HeightField::Block &into);
  [[nodiscard]] std::optional<double> AslMAt(int zoom, LongitudeLatitude at) const;
  [[nodiscard]] static std::optional<double>
  AslMAt(std::span<const Entry> fields, int zoom, LongitudeLatitude at);

private:
  std::vector<Entry> Fields_;
};

}

#endif

#ifndef OUTSHINE_WORLD_DATA_TILESOURCEIDENTITY_H
#define OUTSHINE_WORLD_DATA_TILESOURCEIDENTITY_H

#include <string>

#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

struct TileSourceIdentity {
  enum class Origin { Provider, Direct, Declared, Shaped };

  Origin From = Origin::Provider;
  DataKind Kind = DataKind::Elevation;
  TileId Tile;
  std::string SourceId;
  std::string Revision;

  [[nodiscard]] bool operator==(const TileSourceIdentity &) const noexcept = default;
};

}
#endif

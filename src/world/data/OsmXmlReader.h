#ifndef OUTSHINE_WORLD_DATA_OSMXMLREADER_H
#define OUTSHINE_WORLD_DATA_OSMXMLREADER_H

#include <cstdint>
#include <expected>
#include <string_view>

#include "OsmElements.h"

namespace outshine::Data {

enum class OsmXmlError : uint8_t {
  InvalidSourceIdentity,
  InvalidDocument,
  UnsupportedRoot,
  InvalidId,
  InvalidCoordinate,
  InvalidTag,
  InvalidMember,
  DuplicateElement
};

class OsmXmlReader {
public:
  [[nodiscard]] static std::expected<OsmElements, OsmXmlError> Read(std::string_view xml,
                                                                    OsmSourceIdentity identity);
};

}

#endif

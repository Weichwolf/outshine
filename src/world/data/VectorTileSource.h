#ifndef OUTSHINE_WORLD_DATA_VECTORTILESOURCE_H
#define OUTSHINE_WORLD_DATA_VECTORTILESOURCE_H

#include <string>

#include "WebTileSource.h"

namespace outshine::Data {

class VectorTileSource : public WebTileSource {
public:
  VectorTileSource(std::string revision,
                   Rank order,
                   AbsencePolicy absence,
                   std::string dataset,
                   const std::string &endpoint);
};

}
#endif

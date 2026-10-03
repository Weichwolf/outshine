#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_VECTORTILESOURCE_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_VECTORTILESOURCE_H

#include <string>

#include "WebTileSource.h"
#include "MvtSchema.h"

namespace outshine::Generators::Osm {

using namespace outshine::Data;

class VectorTileSource : public WebTileSource {
public:
  VectorTileSource(std::string revision,
                   Rank order,
                   AbsencePolicy absence,
                   std::string dataset,
                   const std::string &endpoint,
                   MvtSchema schema = MvtSchema::Shortbread);
};

}
#endif

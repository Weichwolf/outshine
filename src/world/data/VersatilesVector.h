#ifndef OUTSHINE_WORLD_DATA_VERSATILESVECTOR_H
#define OUTSHINE_WORLD_DATA_VERSATILESVECTOR_H

#include <string>

#include "WebTileSource.h"

namespace outshine::Data {

class VersatilesVector : public WebTileSource {
public:
  VersatilesVector(std::string revision,
                   Rank order,
                   AbsencePolicy absence,
                   std::string dataset = {},
                   const std::string &endpoint = {});
};

}
#endif

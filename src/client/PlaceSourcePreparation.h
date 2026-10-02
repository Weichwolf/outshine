#ifndef OUTSHINE_CLIENT_PLACESOURCEPREPARATION_H
#define OUTSHINE_CLIENT_PLACESOURCEPREPARATION_H

#include <Outshine.h>
#include "OsmSourceCachePreparation.h"
#include <functional>

namespace outshine::Client {
[[nodiscard]] Result
PreparePlaceSources(const Scenario::Document &declared,
                    const Roots &roots,
                    double budgetS,
                    const std::function<void(Generators::Osm::SourceCacheProgress)> &progress = {});
}
#endif

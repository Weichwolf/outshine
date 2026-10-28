#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_VECTORTILESOURCE_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_VECTORTILESOURCE_H

#include <string>
#include <optional>

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

protected:
  [[nodiscard]] bool AcceptsIdentity(const Wire::Response &response) const noexcept override;
  [[nodiscard]] bool AcceptsEmptyPayload() const noexcept override;

private:
  std::optional<std::string> ExpectedRelease_;
  bool RevisionEndpoint_ = false;
};

}
#endif

#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_OSMPROVIDER_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_OSMPROVIDER_H

#include <world/Provider.h>
#include <world/SourceProvider.h>
#include <world/data/Source.h>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace outshine::Generators::Osm {
class Provider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "osm"; }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &provider, std::string_view root) const override;
};
}
#endif

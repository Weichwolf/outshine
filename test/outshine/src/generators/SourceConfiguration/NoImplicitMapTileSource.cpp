#include "ShippedProviders.h"
#include "SourceConfiguration.h"
#include "Check.h"

#include <array>
#include <string>

int main() {
  outshine::Data::ProviderRegistry registry;
  outshine::Generators::RegisterShippedProviders(registry);
  using namespace outshine::Data;
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  std::string error;
  CHECK(RegisterSources(sources, outshine::Generators::ShippedProviders(), {}, error, registry),
        error.c_str());
  for (size_t i = 0; i < sources.Count(); ++i) {
    CHECK(sources.At(i).Declaration().Kind != DataKind::VectorMap,
          "shipped source set has no reduced vector-map source");
  }
  const std::array providers{SourceProvider{.Kind = "terrain"}, SourceProvider{.Kind = "vector"}};
  SourceSet rejected(store);
  CHECK(!RegisterSources(rejected, providers, {}, error, registry) && rejected.Count() == 0 &&
            error.find("no default") != std::string::npos,
        "implicit map-tile input fails atomically without registering another source");
  return Report();
}

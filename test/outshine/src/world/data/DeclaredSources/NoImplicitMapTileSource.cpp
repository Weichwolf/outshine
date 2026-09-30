#include "DeclaredSources.h"
#include "Check.h"

#include <array>
#include <string>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  std::string error;
  CHECK(RegisterDeclared(sources, ShippedProviders(), {}, error), error.c_str());
  for (size_t i = 0; i < sources.Count(); ++i) {
    CHECK(sources.At(i).Declaration().Kind != DataKind::VectorMap,
          "shipped source set has no reduced vector-map source");
  }
  const std::array providers{SourceProvider{.Kind = "terrain"}, SourceProvider{.Kind = "vector"}};
  SourceSet rejected(store);
  CHECK(!RegisterDeclared(rejected, providers, {}, error) && rejected.Count() == 0 &&
            error.find("no default") != std::string::npos,
        "implicit map-tile input fails atomically without registering another source");
  return Report();
}

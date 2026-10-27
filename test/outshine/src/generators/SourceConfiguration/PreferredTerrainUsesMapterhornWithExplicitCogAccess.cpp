#include "BuiltinSourceProviders.h"
#include "SourceConfiguration.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  ProviderRegistry registry;
  Generators::RegisterBuiltinSourceProviders(registry);
  const auto primary = ConfigureSource({.Kind = "terrain",
                                        .Revision = {},
                                        .Dataset = {},
                                        .Location = {},
                                        .Endpoint = {},
                                        .Coverage = {},
                                        .PayloadSha256 = {},
                                        .Schema = {}},
                                       {},
                                       registry);
  CHECK(primary.has_value(), "preferred height source configures through the public registry");
  if (primary) {
    const auto &decl = (*primary)->Declaration();
    CHECK(decl.Id == "mapterhorn.terrarium" &&
              decl.Endpoint == "https://tiles.mapterhorn.com/{z}/{x}/{y}.webp" &&
              decl.Kind == DataKind::Elevation && decl.How == Scheme::TileZxy &&
              decl.Wire == WireFormat::TerrariumWebp && decl.AncestorFill,
          "default terrain is the measured preferred lossless WebP source");
  }
  const auto original = ConfigureSource({.Kind = "copernicus",
                                         .Revision = {},
                                         .Dataset = {},
                                         .Location = {},
                                         .Endpoint = {},
                                         .Coverage = {},
                                         .PayloadSha256 = {},
                                         .Schema = {}},
                                        {},
                                        registry);
  CHECK(original.has_value(), "library users can explicitly select the original COG provider");
  if (original) {
    CHECK((*original)->Declaration().Wire == WireFormat::CopernicusCog &&
              (*original)->Declaration().How == Scheme::GeographicCell,
          "original native source remains distinct without a client fallback");
  }
  const auto invalid = ConfigureSource({.Kind = "terrain",
                                        .Revision = {},
                                        .Dataset = "invalid",
                                        .Location = {},
                                        .Endpoint = "http://fixture.invalid/{z}/{x}/{y}.webp",
                                        .Coverage = {},
                                        .PayloadSha256 = {},
                                        .Schema = {}},
                                       {},
                                       registry);
  CHECK(!invalid, "plain HTTP cannot bypass adapter validation");
  const auto replacement = ConfigureSource({.Kind = "terrain",
                                            .Revision = {},
                                            .Dataset = "alternate",
                                            .Location = {},
                                            .Endpoint = "https://fixture.invalid/{z}/{x}/{y}.webp",
                                            .Coverage = {},
                                            .PayloadSha256 = {},
                                            .Schema = {}},
                                           {},
                                           registry);
  CHECK(replacement &&
            (*replacement)->Declaration().Endpoint == "https://fixture.invalid/{z}/{x}/{y}.webp" &&
            (*replacement)->Declaration().Id == "alternate",
        "another endpoint and dataset reuse the same format provider through configuration");
  return Report();
}

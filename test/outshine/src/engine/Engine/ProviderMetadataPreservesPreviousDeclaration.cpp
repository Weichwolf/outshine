#include "Engine.h"
#include "ScenarioRead.h"
#include "GeographicBounds.h"
#include <expected>
#include <optional>
#include <cstddef>
#include "Check.h"

#include <array>
#include <string>
#include <string_view>

namespace {
constexpr double kWestDeg = 8.54;

std::expected<void, std::string> DeclareXml(outshine::Engine &engine, std::string_view text) {
  outshine::Scenario::Document candidate;
  std::string error;
  if (!outshine::ReadScenario(text.data(), text.size(), candidate, error)) {
    return std::unexpected(error);
  }
  return engine.declare(candidate);
}

std::string Changed(std::string text, std::string_view from, std::string_view to) {
  const size_t at = text.find(from);
  if (at != std::string::npos) { text.replace(at, from.size(), to); }
  return text;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const std::string valid =
      "<scenario name='source'><providers>"
      "<provider kind='osm' dataset='openstreetmap' location='region.osm' pin='r1' "
      "rank='0' whenAbsent='fail' westDeg='8.54' southDeg='49.315' "
      "eastDeg='8.61' northDeg='49.34'/>"
      "</providers></scenario>";
  const std::array invalid{
      Changed(valid, "pin='r1' ", ""),
      Changed(valid, "dataset='openstreetmap' ", ""),
      Changed(valid, "location='region.osm' ", ""),
      Changed(valid, "whenAbsent='fail' ", ""),
      Changed(valid, "westDeg='8.54' ", ""),
      Changed(valid, "eastDeg='8.61' ", ""),
      Changed(valid, "eastDeg='8.61'", "eastDeg='nan'"),
      Changed(valid, "westDeg='8.54'", "westDeg='179'"),
      Changed(valid, "location='region.osm'", "location='file://region.osm'"),
      Changed(valid,
              "</providers>",
              "<provider kind='osm' dataset='openstreetmap' location='other.osm' pin='r1' "
              "rank='0' whenAbsent='fail' westDeg='8.54' southDeg='49.315' "
              "eastDeg='8.61' northDeg='49.34'/></providers>"),
      Changed(valid,
              "</providers>",
              "<provider kind='osm' dataset='other' location='other.osm' pin='r1' "
              "rank='1' whenAbsent='fail' westDeg='8.54' southDeg='49.315' "
              "eastDeg='8.61' northDeg='49.34'/></providers>"),
      std::string{"<scenario><providers><provider kind='vector' endpoint='http://a/{z}/{x}/{y}' "
                  "dataset='a'/></providers></scenario>"},
      std::string{"<scenario><providers><provider kind='vector' endpoint='https://a/{z}/{x}/{y}'/>"
                  "</providers></scenario>"},
      std::string{"<scenario><providers><provider kind='osm' endpoint='https://a/{z}/{x}/{y}'/>"
                  "</providers></scenario>"},
  };

  Engine engine;
  Scenario::Document document;
  document.Named.Name = "previous";
  document.Providers.push_back({.Kind = "vector",
                                .Revision = "held",
                                .Dataset = "held",
                                .Location = {},
                                .Endpoint = "https://held.example/{z}/{x}/{y}.pbf",
                                .Coverage = {},
                                .PayloadSha256 = {},
                                .Schema = "openmaptiles"});
  CHECK(engine.declare(document), "previous source declaration is valid without acquiring data");
  std::string error;
  for (const std::string &text : invalid) {
    const auto result = DeclareXml(engine, text);
    if (!result) { error = result.error(); }
    document = engine.declaration();
    CHECK(!result, "invalid domain metadata is rejected before setup");
    CHECK(!error.empty() && document.Named.Name == "previous" && document.Providers.size() == 1 &&
              document.Providers.front().Kind == "vector",
          "invalid provider preserves the prior declaration");
  }
  CHECK(DeclareXml(engine, valid), "valid bounded source declaration succeeds after refusal");
  document = engine.declaration();
  const auto coverage = document.Providers.empty() ? std::optional<GeographicBounds>{}
                                                   : document.Providers.front().Coverage;
  CHECK(document.Providers.size() == 1 && coverage.has_value() &&
            coverage.value().WestDeg == kWestDeg &&
            document.Providers.front().Dataset == "openstreetmap" &&
            document.Providers.front().Location == "region.osm",
        "source identity and bounds reach the native declaration");
  const std::string catalogue =
      "<scenario><providers><provider kind='osm' dataset='openstreetmap.original' "
      "endpoint='https://api.openstreetmap.org/api/0.6' pin='cells-r1' "
      "whenAbsent='fail'/></providers></scenario>";
  CHECK(DeclareXml(engine, catalogue) && engine.declaration().Providers.size() == 1 &&
            !engine.declaration().Providers.front().Coverage &&
            engine.declaration().Providers.front().Endpoint ==
                "https://api.openstreetmap.org/api/0.6",
        "an official catalogue declares a source whose individual requests supply bounded cells");
  return Report();
}

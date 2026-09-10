#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array modes{Scenario::AssetAnimation::Play,
                             Scenario::AssetAnimation::Loop,
                             Scenario::AssetAnimation::Ignore,
                             Scenario::AssetAnimation::Driven};
  const std::array names{"play", "loop", "ignore", "driven"};
  for (size_t i = 0; i < names.size(); ++i) {
    const std::string input =
        std::string("<scenario><assets><asset uri='a&amp;b.glb' kind='gltf' "
                    "digest='pinned' variant='winter' clip='3' animation='") +
        names[i] +
        "'><wears named='bark' node='trunk' part='2' keepsMaps='yes'><row r='0.25' g='0.5' "
        "b='0.75' a='0.5' metalness='0.25' roughness='0.75' emissionR='1' emissionG='2' "
        "emissionB='3' unlit='yes' doubleSided='yes' coverageCut='0.25'/></wears>"
        "</asset></assets></scenario>";
    Scenario::Document document;
    std::string error;
    CHECK(ReadScenario(input.data(), input.size(), document, error), "independent fixture parses");
    for (int pass = 0; pass < 2; ++pass) {
      CHECK(document.Assets.size() == 1, "one asset retained");
      if (document.Assets.size() != 1) { continue; }
      const auto &asset = document.Assets.front();
      CHECK(asset.Uri == "a&b.glb" && asset.Kind == "gltf" && asset.Digest == "pinned" &&
                asset.Variant == "winter" && asset.Clip == 3 && asset.Animation == modes[i],
            "asset metadata and playback match the independent fixture");
      CHECK(asset.Surfaces.size() == 1, "material override retained");
      if (asset.Surfaces.size() == 1) {
        const auto &surface = asset.Surfaces.front();
        const auto &row = surface.Row;
        CHECK(surface.Named == "bark" && surface.Node == "trunk" && surface.Part == 2 &&
                  surface.KeepsMaps,
              "override selectors retained");
        CHECK(row.BaseColour[0] == 0.25f && row.BaseColour[1] == 0.5f &&
                  row.BaseColour[2] == 0.75f && row.BaseColour[3] == 0.5f &&
                  row.Metalness == 0.25f && row.Roughness == 0.75f && row.Emission[0] == 1.0f &&
                  row.Emission[1] == 2.0f && row.Emission[2] == 3.0f && row.Unlit &&
                  row.DoubleSided && row.CoverageCut == 0.25f,
              "all supported material fields retain independently specified values");
      }
      if (pass == 0) {
        const auto output = WriteScenario(document);
        CHECK(output.has_value(), "scenario export succeeds");
        if (!output) { return Report(); }
        CHECK(ReadScenario(output->data(), output->size(), document, error),
              "written scene parses");
      }
    }
  }
  return Report();
}

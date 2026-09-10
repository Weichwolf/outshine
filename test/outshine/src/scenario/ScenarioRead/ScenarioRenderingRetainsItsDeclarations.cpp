#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <string>
#include <string_view>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const std::string_view outputTag : {"output", "keep"}) {
    const std::string input =
        "<scenario name='a&amp;b' version='2' active='winter' epoch='123.5' decay='0.25'>"
        "<render widthPx='640' heightPx='360' fps='30' fill='0.75' audits='yes' "
        "orbitDegPerFrame='0.5' transfer='filmic' exposure='1.25' precision='half'>" +
        std::string("<") + std::string(outputTag) + " name='sceneColour'/><" +
        std::string(outputTag) +
        " name='depth&amp;mask'/><stage name='terrain'/>"
        "<stage name='lighting'/></render><lighting shadowRadiusM='12.5'>"
        "<key lux='7500' elevationDeg='25' bearingDeg='135'/>"
        "<environment r='0.125' g='0.25' b='0.5'/></lighting></scenario>";
    Scenario::Document document;
    std::string error;
    CHECK(ReadScenario(input.data(), input.size(), document, error), "independent fixture parses");
    for (int pass = 0; pass < 2; ++pass) {
      const auto &identity = document.Named;
      CHECK(identity.Name == "a&b" && identity.Version == "2" && identity.Active == "winter" &&
                identity.Epoch == 123.5 && identity.Decay == 0.25,
            "identity retains every field");
      const auto &render = document.Render;
      CHECK(render.Declared && render.Frame.WidthPx == 640 && render.Frame.HeightPx == 360 &&
                render.Fps == 30 && render.Fill == 0.75 && render.Audits &&
                render.OrbitDegPerFrame == 0.5 && render.Transfer == "filmic" &&
                render.Exposure == 1.25 && render.Precision == "half",
            "render settings retained");
      CHECK(render.Outputs == std::vector<std::string>({"sceneColour", "depth&mask"}) &&
                render.Stages == std::vector<std::string>({"terrain", "lighting"}),
            "ordered output and stage names retain exact content");
      const auto &lighting = document.Lit;
      CHECK(lighting.Declared && lighting.ShadowRadiusM == 12.5 && lighting.Key.Lux == 7500 &&
                lighting.Key.ElevationDeg == 25 && lighting.Key.BearingDeg == 135 &&
                lighting.IndirectLight == (Vec3{{0.125, 0.25, 0.5}}),
            "lighting settings retained");
      if (pass == 0) {
        const auto written = WriteScenario(document);
        CHECK(ReadScenario(written.data(), written.size(), document, error),
              "written scene parses");
      }
    }
  }
  for (const std::string_view input :
       {"<scenario/>", "<scenario><render/><lighting/></scenario>"}) {
    Scenario::Document document;
    std::string error;
    CHECK(ReadScenario(input.data(), input.size(), document, error), "empty declarations parse");
    const bool declared = input.find("render") != std::string_view::npos;
    const auto written = WriteScenario(document);
    CHECK(ReadScenario(written.data(), written.size(), document, error),
          "empty declarations roundtrip");
    CHECK(document.Render.Declared == declared && document.Lit.Declared == declared &&
              document.Render.Outputs.empty() && document.Render.Stages.empty(),
          "absent sections stay absent; empty lists stay empty");
  }
  return Report();
}

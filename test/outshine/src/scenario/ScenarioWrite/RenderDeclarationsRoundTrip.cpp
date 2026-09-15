#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <string>
#include <string_view>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view input = R"(<scenario><render widthPx="1234" heightPx="567" fps="59.94"
    fill="0.8" audits="yes" orbitDegPerFrame="0.12345678901234567" transfer="filmic" exposure="1.25" precision="float"
    leftFrac="0.1" topFrac="0.2" widthFrac="0.3" heightFrac="0.4">
    <keep name="depth"/><keep name="normal"/><keep name="depth"/>
    <stage name="a&amp;b"/><stage name="next"/></render></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), source, error), error.c_str());
  CHECK(source.Render.Outputs == std::vector<std::string>({"depth", "normal", "depth"}),
        "legacy keep aliases preserve order and duplicates");
  CHECK(source.Render.Picture.WidthFrac == 0.3 && source.Render.Picture.HeightFrac == 0.4,
        "declared image rectangle imports");
  auto check = [&] {
    const auto written = WriteScenario(source);
    CHECK(written.has_value(), "render settings export");
    if (!written) { return; }
    CHECK(written->find("<keep") == std::string::npos, "legacy aliases canonicalize to output");
    Scenario::Document copy;
    CHECK(ReadScenario(written->data(), written->size(), copy, error), error.c_str());
    const auto &a = source.Render;
    const auto &b = copy.Render;
    CHECK(a.Declared == b.Declared && a.Frame.WidthPx == b.Frame.WidthPx &&
              a.Frame.HeightPx == b.Frame.HeightPx && a.Fps == b.Fps && a.Fill == b.Fill &&
              a.OrbitDegPerFrame == b.OrbitDegPerFrame && a.Audits == b.Audits,
          "render dimensions and numerical settings survive");
    CHECK(a.Picture.LeftFrac == b.Picture.LeftFrac && a.Picture.TopFrac == b.Picture.TopFrac &&
              a.Picture.WidthFrac == b.Picture.WidthFrac &&
              a.Picture.HeightFrac == b.Picture.HeightFrac,
          "image rectangle retains exact values");
    CHECK(a.Outputs == b.Outputs && a.Stages == b.Stages && a.Transfer == b.Transfer &&
              a.Exposure == b.Exposure && a.Precision == b.Precision,
          "ordered outputs stages and radiance settings survive");
  };
  check();
  source.Render.Outputs.clear();
  source.Render.Stages.clear();
  check();
  for (const std::string_view text :
       {"<scenario><drive/></scenario>",
        "<scenario><drive fromLat=\"47\" fromLon=\"11\" toLat=\"48\" toLon=\"12\"/></scenario>"}) {
    CHECK(!ReadScenario(text.data(), text.size(), source, error),
          "unsupported root route is explicitly rejected");
    CHECK(error.find("unsupported") != std::string::npos,
          "route failure states missing capability");
    CHECK(source.Render.Frame.WidthPx == 1234, "rejected route preserves prior declaration");
  }
  return Report();
}

#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <cmath>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view input = R"(<scenario><regions>
    <region id="A&amp;&quot;" kind="district" x="-12.25" y="3" z="4.5" radiusM="12.125" streams="no">
      <uses what="terrain&amp;roads"/><uses what="text&#9;with&#10;space"/><uses what="terrain&amp;roads"/>
    </region>
    <region id="B"/>
    <door id="forward" from="A&amp;&quot;" to="B" x="1.25" y="-2.5" z="3.75"/>
    <door id="reverse" from="B" to="A&amp;&quot;" x="-1.25"/>
  </regions></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), source, error), "independent XML fixture imports");
  CHECK(source.Regions.size() == 2 && source.Doors.size() == 2,
        "reader retains both regions and directed transitions");
  if (source.Regions.size() != 2 || source.Doors.size() != 2) { return Report(); }
  CHECK(!source.Regions[0].Streams && source.Regions[1].Streams,
        "explicit false and default true differ");
  CHECK(source.Regions[0].Uses[1] == "text\twith\nspace", "resource reference escapes decode");
  source.Regions[0].OriginM[0] = std::nextafter(6000000.0, 7000000.0);
  source.Doors[0].AtM[2] = std::nextafter(0.125, 1.0);
  const auto check = [&](const Scenario::Document &original) {
    const auto written = WriteScenario(original);
    CHECK(written.has_value(), "region metadata exports");
    if (!written) { return; }
    Scenario::Document copy;
    CHECK(ReadScenario(written->data(), written->size(), copy, error), "exported metadata parses");
    CHECK(copy.Regions.size() == original.Regions.size(), "region count retained");
    CHECK(copy.Doors.size() == original.Doors.size(), "transition count retained");
    if (copy.Regions.size() != original.Regions.size() ||
        copy.Doors.size() != original.Doors.size()) {
      return;
    }
    for (size_t i = 0; i < original.Regions.size(); ++i) {
      const auto &a = original.Regions[i];
      const auto &b = copy.Regions[i];
      CHECK(a.Id == b.Id && a.Kind == b.Kind && a.RadiusM == b.RadiusM && a.Streams == b.Streams &&
                a.Uses == b.Uses,
            "region values, resource order and duplicates preserved");
      for (int axis = 0; axis < 3; ++axis) {
        CHECK(a.OriginM[axis] == b.OriginM[axis], "exact region coordinates preserved");
      }
    }
    for (size_t i = 0; i < original.Doors.size(); ++i) {
      const auto &a = original.Doors[i];
      const auto &b = copy.Doors[i];
      CHECK(a.Id == b.Id && a.From == b.From && a.To == b.To,
            "ordered transition endpoints and labels preserved");
      for (int axis = 0; axis < 3; ++axis) {
        CHECK(a.AtM[axis] == b.AtM[axis], "exact transition coordinates preserved");
      }
    }
  };
  check(source);
  source.Regions.clear();
  check(source);
  source.Doors.clear();
  check(source);
  const auto empty = WriteScenario(source);
  CHECK(empty && empty->find("<regions>") == std::string::npos,
        "empty metadata does not emit a section");
  return Report();
}

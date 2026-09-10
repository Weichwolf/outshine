#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <array>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const size_t room : {size_t{0}, size_t{65536}, std::numeric_limits<size_t>::max()}) {
    Scenario::Document source;
    source.Room = room;
    source.Surfaces.push_back({.Document = "<button onclick=\"go()\">A&B</button>\n\t",
                               .Style = "button { color: red; }\r\n",
                               .Programme = "var label = 'a&b';\n",
                               .Where = {.LeftFrac = -0.123456789012345,
                                         .TopFrac = 0.987654321098765,
                                         .WidthFrac = 0.25,
                                         .HeightFrac = 0.5},
                               .Z = std::numeric_limits<int>::max()});
    source.Surfaces.push_back({.Document = "<div/>", .Z = std::numeric_limits<int>::min()});
    const auto text = WriteScenario(source);
    CHECK(text.has_value(), "capacity and UI declaration written");
    if (!text) { continue; }
    Scenario::Document restored;
    std::string error;
    CHECK(ReadScenario(text->data(), text->size(), restored, error), error.c_str());
    CHECK(restored.Room == source.Room, "capacity retains every integer bit");
    CHECK(restored.Surfaces.size() == source.Surfaces.size(), "all UI declarations retained");
    if (restored.Surfaces.size() != source.Surfaces.size()) { continue; }
    for (size_t at = 0; at < source.Surfaces.size(); ++at) {
      const auto &before = source.Surfaces[at];
      const auto &after = restored.Surfaces[at];
      CHECK(before.Document == after.Document && before.Style == after.Style &&
                before.Programme == after.Programme && before.Z == after.Z,
            "source strings, ordering and signed stacking limits retained");
      CHECK(before.Where.LeftFrac == after.Where.LeftFrac &&
                before.Where.TopFrac == after.Where.TopFrac &&
                before.Where.WidthFrac == after.Where.WidthFrac &&
                before.Where.HeightFrac == after.Where.HeightFrac,
            "normalized patch values retain precision");
    }
    CHECK(WriteScenario(restored) == text, "serialized declaration is stable after reading");
  }
  Scenario::Document invalid;
  invalid.Surfaces.emplace_back();
  CHECK(!WriteScenario(invalid), "empty required UI document rejected by writer");
  const auto empty = WriteScenario({});
  CHECK(empty && empty->find("<scene") == std::string::npos &&
            empty->find("<surfaces>") == std::string::npos,
        "default capacity and absent surfaces need no sections");
  return Report();
}

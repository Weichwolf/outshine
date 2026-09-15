#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "ScenarioLayer.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view xml =
      R"(<scenario active="day night"><layer id="one&amp;two" path="dir/a&amp;b.scn" set="day"/><layer path="../always.scn"/><layer id="off" path="/absolute.scn" set="winter"/><layer path="same.scn" set="day&#9;night"/></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(xml.data(), xml.size(), source, error),
        "raw XML imports without loading references");
  CHECK(source.Layers.size() == 4, "all source references retained");
  const auto written = WriteScenario(source);
  CHECK(written.has_value(), "raw source exports");
  if (!written) { return Report(); }
  Scenario::Document copy;
  CHECK(ReadScenario(written->data(), written->size(), copy, error), "exported source parses");
  CHECK(copy.Layers.size() == source.Layers.size(), "reference count retained");
  CHECK(copy.Named.Active == source.Named.Active, "active set retained");
  if (copy.Layers.size() != source.Layers.size()) { return Report(); }
  for (size_t i = 0; i < source.Layers.size(); ++i) {
    const auto &a = source.Layers[i];
    const auto &b = copy.Layers[i];
    CHECK(a.Id == b.Id && a.Path == b.Path && a.Set == b.Set,
          "ordered references and escaped strings retained");
    CHECK(LayerActive(a, source.Named.Active) == LayerActive(b, copy.Named.Active),
          "activation semantics retained");
  }
  const auto empty = WriteScenario({});
  CHECK(empty && empty->find("<layer ") == std::string::npos, "empty source has no references");
  return Report();
}

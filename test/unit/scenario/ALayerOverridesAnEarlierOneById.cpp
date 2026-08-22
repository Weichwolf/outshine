#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "ScenarioLayer.h"
#include "ScenarioRead.h"

using outshine::Layer;
using outshine::LayerActive;
using outshine::MergeLayer;
using outshine::ReadScenario;
using outshine::Scenario;

namespace {

[[nodiscard]] bool Parsed(const char *text, Scenario &out, std::string &error) {
  return ReadScenario(text, std::string(text).size(), out, error);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  Scenario base;
  CHECK(Parsed("<scenario name=\"game\" active=\"winter\">"
               "<kinds><kind name=\"mug\"><has name=\"volumeL\" value=\"0.5\"/></kind>"
               "<kind name=\"crate\"/></kinds>"
               "<instances><instance of=\"mug\" id=\"cup\"/></instances>"
               "</scenario>",
               base, error),
        "a base declaration parses");

  Scenario mod;
  CHECK(Parsed("<scenario name=\"mod\">"
               "<kinds><kind name=\"mug\"><has name=\"volumeL\" value=\"0.7\"/></kind>"
               "<kind name=\"lantern\"/></kinds>"
               "<instances><instance of=\"lantern\" id=\"lamp\"/></instances>"
               "</scenario>",
               mod, error),
        "and a mod layer parses through the SAME reader -- one grammar, no second dialect");

  std::vector<std::string> trace;
  CHECK(MergeLayer(base, mod, "mod", trace, error), "the layer merges");
  CHECK(base.Kinds.size() == 3 && base.Kinds[0].Name == "mug" &&
            base.Kinds[0].Attributes.size() == 1 && base.Kinds[0].Attributes[0].Value == "0.7",
        "**A LATER LAYER OVERRIDES AN EARLIER ONE BY ID, PER ROW**: the mod's mug replaced "
        "the base's whole row -- 0.7 L now, in the base row's place, order preserved");
  CHECK(base.Kinds[2].Name == "lantern" && base.Instances.size() == 2,
        "**AN UNKNOWN ID IS ADDING, NOT FAILING** -- that is how a mod adds a thing");
  bool overrode = false, added = false;
  for (const std::string &row : trace) {
    if (row.find("overrode kind 'mug'") != std::string::npos) { overrode = true; }
    if (row.find("added kind 'lantern'") != std::string::npos) { added = true; }
  }
  CHECK(overrode && added,
        "**AND WHAT OVERRODE WHAT IS PUBLISHABLE** -- a declaration nobody can trace is a "
        "declaration nobody can debug");

  Scenario nested;
  CHECK(Parsed("<scenario name=\"deep\"><layer path=\"more.xml\"/></scenario>", nested, error),
        "a layer declaring layers parses as XML");
  CHECK(!MergeLayer(base, nested, "deep", trace, error) &&
            error.find("one level") != std::string::npos,
        "**A LAYER'S OWN LAYERS ARE REFUSED, ONE LEVEL** -- a graph of overrides is a thing "
        "nobody can predict");

  CHECK(LayerActive(Layer{"a", "a.xml", ""}, "winter") &&
            LayerActive(Layer{"b", "b.xml", "winter"}, "winter alpine") &&
            !LayerActive(Layer{"c", "c.xml", "summer"}, "winter"),
        "**A CHANGE SET SELECTS**: an unset layer always stands, a set layer stands when the "
        "declaration's active list names its set -- one scenario carries variants without a "
        "second copy of itself");

  Covers("III.6 a layer overrides an earlier one by id: per row by the grammar's own "
         "identity attribute, unknown ids add, one level only, the order is the "
         "declaration's, a change set selects, and the merge publishes its trace "
         "(board:1493)");
  return Report();
}

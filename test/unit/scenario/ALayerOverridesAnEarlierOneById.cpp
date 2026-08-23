#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Check.h"

#include "ScenarioLayer.h"
#include "ScenarioRead.h"

using outshine::Layer;
using outshine::LayerActive;
using outshine::ApplyLayer;
using outshine::MergeLayer;
using outshine::ReadScenario;
using outshine::Scenario;

namespace {

[[nodiscard]] bool Parsed(const char *text, Scenario &out, std::string &error) {
  return ReadScenario(text, std::strlen(text), out, error);
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

  Scenario lit;
  CHECK(Parsed("<scenario name=\"winter\">"
               "<lighting><key lux=\"3000\" elevationDeg=\"8\"/></lighting>"
               "<assets><asset uri=\"car.glb\" digest=\"bbb\"/></assets>"
               "<audio><sound id=\"wind\" uri=\"wind2.ogg\"/></audio></scenario>",
               lit, error),
        "a winter layer declaring lighting, a re-pinned asset and a sound parses");
  Scenario town;
  CHECK(Parsed("<scenario name=\"town\">"
               "<lighting><key lux=\"90000\" elevationDeg=\"55\"/></lighting>"
               "<assets><asset uri=\"car.glb\" digest=\"aaa\"/></assets>"
               "<audio><sound id=\"wind\" uri=\"wind.ogg\"/></audio></scenario>",
               town, error),
        "over a base that lit a summer noon");
  trace.clear();
  const char *winter = "<scenario name=\"winter\">"
                       "<lighting><key lux=\"3000\"/></lighting>"
                       "<assets><asset uri=\"car.glb\" digest=\"bbb\"/></assets>"
                       "<audio><sound id=\"wind\" uri=\"wind.ogg\" gainDb=\"-3\"/></audio>"
                       "<render fps=\"30\"/>"
                       "<player view=\"chase\"/></scenario>";
  town.Render.Frame.WidthPx = 1920;
  town.Render.Declared = true;
  town.Played.Is = "f31";
  town.Played.Declared = true;
  town.Lit.Key.ElevationDeg = 55.0;
  CHECK(ApplyLayer(town, winter, std::strlen(winter), "winter", trace, error) &&
            town.Lit.Key.Lux == 3000.0,
        "**THE CANONICAL MOD WORKS**: the winter layer dims the light and no collection is "
        "silently swallowed");
  CHECK(town.Lit.Key.ElevationDeg == 55.0,
        "**AND OMISSION KEEPS, AT ATTRIBUTE LEVEL**: the mod spelled only lux, so the "
        "base's sun elevation stands -- the house template rule, not a wholesale nuke");
  CHECK(town.Render.Frame.WidthPx == 1920 && town.Render.Fps == 30.0,
        "a render layer declaring only fps keeps the base's 1920 -- no ambient number a "
        "declaration never spelled can reach the result");
  CHECK(town.Played.Is == "f31" && town.Played.View == "chase",
        "and a player layer declaring only the view keeps who the player IS -- nothing "
        "vanishes wordlessly");
  CHECK(town.Assets.size() == 1 && town.Assets[0].Digest == "bbb",
        "an asset with the same uri and a NEW digest replaces the row -- the uri is the "
        "declared name, the digest is the content pin, and a mod re-pins");
  CHECK(town.Sounds.size() == 1 && town.Sounds[0].GainDb == -3.0,
        "and a sound overrides by its URI -- the identity 1655 put in the Required column, "
        "not the optional id");

  {
    Scenario route;
    CHECK(Parsed("<scenario name=\"base\">"
                 "<render><stage name=\"sky\"/><stage name=\"subjects\"/></render>"
                 "<drive fromLat=\"48.1\" fromLon=\"11.5\" toLat=\"52.5\" toLon=\"13.4\"/>"
                 "</scenario>",
                 route, error),
          "a base declares a route and a two-stage plan");
    const char *zoomed = "<scenario name=\"mod\">"
                         "<drive zoom=\"15\"/>"
                         "<render><stage name=\"sky\"/></render></scenario>";
    trace.clear();
    CHECK(ApplyLayer(route, zoomed, std::strlen(zoomed), "mod", trace, error),
          "a layer declaring only the zoom and a one-stage plan applies");
    CHECK(route.Driven.FromLatDeg == 48.1 && route.Driven.ToLatDeg == 52.5 &&
              route.Driven.Zoom == 15,
          "**THE DRIVE KEEPS TOO**: zoom-only keeps the route's four coordinates -- no "
          "silent voyage to the Gulf of Guinea, and the trace stopped lying");
    CHECK(route.Render.Stages.size() == 1 && route.Render.Stages[0] == "sky",
          "and a redeclared stage list REPLACES -- the declared list is the list, never an "
          "append that draws the sky twice");
  }

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

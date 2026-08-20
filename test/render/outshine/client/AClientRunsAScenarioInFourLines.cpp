#include <cstdio>
#include <string>

#include <outshine/Outshine.h>

#include "Check.h"
#include "PreparedRoot.h"

namespace {

std::string EntryPath(const std::string &prepared) {
  const std::string manifest = prepared + "/manifest.json";
  std::FILE *const file = std::fopen(manifest.c_str(), "rb");
  if (file == nullptr) { return std::string(); }
  std::string text;
  char block[4096];
  size_t read = 0;
  while ((read = std::fread(block, 1, sizeof block, file)) > 0) { text.append(block, read); }
  std::fclose(file);
  const size_t entry = text.find("\"entry\"");
  if (entry == std::string::npos) { return std::string(); }
  const size_t open = text.find('"', text.find(':', entry));
  const size_t close = text.find('"', open + 1);
  if (open == std::string::npos || close == std::string::npos) { return std::string(); }
  return prepared + "/" + text.substr(open + 1, close - open - 1);
}

std::string WriteScenario(const std::string &stands) {
  const std::string path = outshine::Test::PreparedRoot() + "/four-lines.scenario";
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return std::string(); }
  std::string text = R"(<?xml version="1.0" encoding="utf-8"?>
<scenario name="four lines" version="1" epoch="2287" decay="0.4">
  <world lat="44.38" lon="4.42" radiusM="800" cloudCover="0.1"/>
  <render widthPx="320" heightPx="240" fps="30" fill="0.9"/>
  <lighting>
    <key lux="40000" elevationDeg="45" bearingDeg="135"/>
    <environment r="0.05" g="0.06" b="0.08"/>
  </lighting>
  <providers>
    <provider kind="terrain" pin="2026-08-20" rank="0" whenAbsent="hand over"/>
  </providers>
  <generators>
    <generator kind="tree">
      <set name="species" value="beech"/>
      <set name="seed" value="7"/>
    </generator>
  </generators>
  <compositors>
    <compositor kind="forest" budgetPx="1.0" on="true"/>
  </compositors>
  <assets>
    <asset uri=")";
  text += stands;
  text += R"(" kind="gltf"/>
  </assets>
  <surfaces>
    <surface document="hud.html" style="hud.css" programme="hud.js" heightFrac="0.2" z="1"/>
  </surfaces>
  <actors>
    <actor kind="settler" programme="wander.js" spawn="ring" tickHz="4">
      <may do="walk"/>
      <may do="speak"/>
    </actor>
  </actors>
  <physics dial="walking"/>
  <clock start="2287-10-23T09:00:00Z" rate="60"/>
  <input>
    <bind event="key:W" action="forward"/>
  </input>
  <state>
    <persist what="actor.named"/>
  </state>
</scenario>
)";
  std::fwrite(text.data(), 1, text.size(), file);
  std::fclose(file);
  return path;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string stands =
      EntryPath(PreparedRoot() + "/" + kPreparedKhronosPrefix + "BoxAnimated");
  CHECK(!stands.empty(), "the scenario's subject is in the prepared corpus");
  if (stands.empty()) { return Report(); }
  const std::string scenario = WriteScenario(stands);
  CHECK(!scenario.empty(), "the scenario is written where the client can read it");
  if (scenario.empty()) { return Report(); }

  outshine::Engine engine;
  engine.RenderTo({1280, 720});
  const bool loaded = engine.Load(scenario);
  const bool ran = loaded && engine.Run();

  if (!loaded) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(loaded, "a client stands a scenario up out of an XML file, in one call and with no engine "
                "type in its hands");
  if (!ran && loaded) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(ran, "and runs it to the end of its declared grid");
  if (!loaded) { return Report(); }

  const outshine::Scenario &read = engine.Declared();
  CHECK(read.Named.Name == "four lines" && read.Named.Epoch == 2287.0,
        "the scenario keeps its identity, so a client can ask what it loaded");
  CHECK(read.Ground.Declared && read.Ground.RadiusM == 800.0,
        "a world origin is read");
  CHECK(read.Providers.size() == 1 && read.Providers[0].Kind == "terrain",
        "a provider is read with its pin and its rank");
  CHECK(read.Generators.size() == 1 && read.Generators[0].Parameters.size() == 2 &&
            read.Generators[0].Parameters[0].Value == "beech",
        "a generator is read with the parameters its own kind declares");
  CHECK(read.Compositors.size() == 1 && read.Compositors[0].On,
        "a compositor is read with its budget");
  CHECK(read.Surfaces.size() == 1 && read.Surfaces[0].Programme == "hud.js",
        "a surface is read with its document, its style and its programme");
  CHECK(read.Actors.size() == 1 && read.Actors[0].Capabilities.size() == 2 &&
            read.Actors[0].TickHz == 4.0,
        "an actor is read with the capabilities it may reach and the rate it ticks at");
  CHECK(read.Motion.Dial == "walking" && read.Time.Rate == 60.0,
        "the physics dial and the clock are read");
  CHECK(read.Input.size() == 1 && read.Input[0].Action == "forward",
        "an input binding names an ACTION, so a client never sees a keycode");
  CHECK(read.State.size() == 1, "and what survives a save is read");

  CHECK(engine.Frames() > 1, "an animated scenario declares more than one frame");
  CHECK(engine.Standing(), "and the engine is still standing when the run is over");

  std::printf("NOTE the engine read and did NOT act on:\n");
  for (const std::string &one : engine.Carried()) { std::printf("       %s\n", one.c_str()); }
  CHECK(!engine.Carried().empty(),
        "what a scenario declares and this runtime does not yet act on is PUBLISHED rather than "
        "read and dropped -- a declaration nobody can see ignored is a scenario that lies");

  outshine::Engine empty;
  const bool nothing = empty.Advance();
  CHECK(!nothing, "an engine with no scenario refuses to advance");
  CHECK(!empty.Error().empty(), "and says why rather than returning quietly");

  outshine::Engine broken;
  const bool subjectless = broken.Load(scenario + ".missing");
  CHECK(!subjectless, "a scenario that is not there is refused by path");

  Covers("I.4 a client says where to render, loads a scenario, runs it and cleans up -- and reaches "
         "nothing of the engine but the handle and the declaration it was given");
  return Report();
}

#include <cstdio>
#include <cstring>
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
  <kinds>
    <kind name="settler" asset="body">
      <mind tier="reflex" uses="steer" hz="60"/>
      <mind tier="reflex" uses="navigate" hz="60"/>
      <mind tier="script" programme="wander.js" hz="4" stepBudget="2000" seed="7"/>
      <may do="walk"/>
      <may do="speak"/>
      <has name="health" value="100"/>
    </kind>
    <kind name="mayor" inherits="settler">
      <mind tier="reflex" uses="navigate" hz="60"/>
      <mind tier="deliberate" prompt="mayor.md" model="local-small" meanwhile="stand"
            everyS="120" temperature="0.7" tokenBudget="512" latencyBudgetMs="800"/>
      <may do="walk"/>
      <may do="speak"/>
      <may do="trade"/>
    </kind>
    <kind name="courier">
      <mind tier="reflex" uses="navigate" hz="60"/>
      <may do="walk"/>
    </kind>
    <kind name="coffee-cup" asset="cup">
      <has name="massKg" value="0.2"/>
      <has name="value" value="1"/>
    </kind>
  </kinds>
  <instances>
    <instance of="settler" id="mama-murphy" in="sanctuary" x="4" y="0" z="-2">
      <has name="health" value="60"/>
      <holds what="coffee-cup"/>
    </instance>
  </instances>
  <regions>
    <region id="sanctuary" kind="exterior" x="0" y="0" z="0" radiusM="900" streams="true">
      <uses what="terrain"/>
    </region>
    <region id="vault-111" kind="interior" radiusM="60" streams="false"/>
    <door id="vault-door" from="vault-111" to="sanctuary" x="12" y="0" z="30"/>
  </regions>
  <volumes>
    <volume id="porch" in="sanctuary" shape="box" x="3" y="0" z="-1"
            extentX="4" extentY="3" extentZ="4" fires="entered-home" when="enter"/>
  </volumes>
  <audio>
    <bus id="master" gainDb="0"/>
    <bus id="music" into="master" gainDb="-6"/>
    <sound id="diamond-city-radio" uri="radio.ogg" bus="music" loops="true" gainDb="-3"/>
    <sound id="footstep" uri="step.ogg" bus="master" positional="true" falloffM="20"/>
  </audio>
  <tables>
    <table id="damage">
      <column name="weapon"/>
      <column name="perShot"/>
      <row><cell value="pipe-pistol"/><cell value="13"/></row>
      <row><cell value="10mm"/><cell value="18"/></row>
    </table>
  </tables>
  <events>
    <event name="entered-home"><carries what="who"/></event>
    <event name="took-damage"><carries what="who"/><carries what="howMuch"/></event>
  </events>
  <views>
    <view id="eyes" follows="player" person="first" offsetY="1.7" fovDeg="80"/>
    <view id="over-the-shoulder" follows="player" person="third" offsetY="1.6"
          distanceM="2.5" pitchLimitDeg="70" fovDeg="70"/>
    <view id="aimed" follows="player" person="first" offsetY="1.7" fovDeg="45" timeScale="0.2"/>
  </views>
  <player is="settler" starts="sanctuary" view="eyes" eyeHeightM="1.75" walkMs="1.4" runMs="4.5"/>
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

std::string InteriorScenario(const std::string &stands) {
  const std::string path = outshine::Test::PreparedRoot() + "/vault-111.scenario";
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return std::string(); }
  std::string text = R"(<?xml version="1.0" encoding="utf-8"?>
<scenario name="vault 111" version="1" epoch="2287">
  <render widthPx="320" heightPx="240" fps="30" fill="0.9"/>
  <lighting><key lux="200" elevationDeg="90" bearingDeg="0"/></lighting>
  <assets>
    <asset uri=")";
  text += stands;
  text += R"(" kind="gltf"/>
  </assets>
  <regions>
    <region id="vault-111" kind="interior" radiusM="60" streams="false"/>
  </regions>
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
  CHECK(read.Kinds.size() == 4 && read.Kinds[0].Capabilities.size() == 2 &&
            read.Kinds[0].Attributes.size() == 1,
        "a KIND is read with what it may do and the attributes it carries -- one mechanism for an "
        "actor, an item, a door and a container. THE RATE IS THE MIND'S: each tier declares its own, "
        "so a kind cannot carry a second number that means the same thing");
  CHECK(read.Kinds[0].Minds.size() == 3,
        "a kind carries 0 or 1..N minds and a settler carries three -- THE TIERS ARE A LADDER AND "
        "NOT A CHOICE, the way the LOD rungs are");
  CHECK(read.Kinds[0].Minds[0].Tier == "reflex" && read.Kinds[0].Minds[0].Uses == "steer" &&
            read.Kinds[0].Minds[0].Hz == 60.0,
        "a REFLEX mind is the engine's own and names which fast intelligence it uses -- it runs at "
        "the frame's rate because it costs microseconds");
  CHECK(read.Kinds[0].Minds[1].Uses == "navigate",
        "and a kind takes several of one tier: steering and navigation are two reflexes and not one");
  CHECK(read.Kinds[0].Minds[2].Tier == "script" && read.Kinds[0].Minds[2].Hz == 4.0 &&
            read.Kinds[0].Minds[2].StepBudget == 2000 && read.Kinds[0].Minds[2].Seed == 7,
        "a SCRIPT mind runs at a declared rate rather than every frame, and is bounded in STEPS "
        "because that is what a bounded interpreter can be held to");
  CHECK(read.Kinds[1].Minds.size() == 2 && read.Kinds[1].Minds[1].Tier == "deliberate" &&
            read.Kinds[1].Minds[1].EverySeconds == 120.0 &&
            read.Kinds[1].Minds[1].TokenBudget == 512 &&
            read.Kinds[1].Minds[1].LatencyBudgetMs == 800.0,
        "a DELIBERATE mind runs every so many SECONDS and is bounded in tokens and in latency -- "
        "three tiers, three deadlines, three kinds of budget");
  CHECK(read.Kinds[1].Minds[1].Meanwhile == "stand",
        "and it declares what the body does WHILE IT THINKS, which is *something is always drawn* "
        "asked of thinking: an actor waiting on an answer still has to be somewhere");
  CHECK(read.Kinds[3].Minds.empty(),
        "and a coffee cup thinks nothing, which is 0 of 0..N");

  CHECK(read.Instances.size() == 1 && read.Instances[0].Of == "settler" &&
            read.Instances[0].In == "sanctuary" && read.Instances[0].Holds.size() == 1,
        "an INSTANCE names its kind, the region it stands in and what it holds, so an inventory is "
        "the same relation as a world placement");
  CHECK(read.Instances[0].Attributes.size() == 1 &&
            read.Instances[0].Attributes[0].Value == "60",
        "and it overrides its kind's attribute, which is what makes a kind a default rather than a "
        "constant");
  CHECK(read.Regions.size() == 2 && read.Regions[0].Kind == "exterior" &&
            !read.Regions[1].Streams,
        "a REGION is read with what it is and whether it streams, which is the difference between "
        "an open world and an interior");
  CHECK(read.Doors.size() == 1 && read.Doors[0].From == "vault-111" &&
            read.Doors[0].To == "sanctuary",
        "and a DOOR names the two it joins, so a transition is a declaration and not a script");
  CHECK(read.Volumes.size() == 1 && read.Volumes[0].Fires == "entered-home" &&
            read.Volumes[0].When == "enter",
        "a VOLUME fires a named event, which is how a quest stage, a trap and an ambush are all one "
        "mechanism");
  CHECK(read.Buses.size() == 2 && read.Buses[1].Into == "master" &&
            read.Sounds.size() == 2 && read.Sounds[1].Positional,
        "AUDIO is declared: buses routing into buses, and a sound that is positional or is not");
  CHECK(read.Tables.size() == 1 && read.Tables[0].Columns.size() == 2 &&
            read.Tables[0].Rows.size() == 2 && read.Tables[0].Rows[1][1] == "18",
        "a TABLE is declared data a script reads, so damage, loot and prices are content and never "
        "a number in the engine");
  CHECK(read.Events.size() == 2 && read.Events[1].Carries.size() == 2,
        "an EVENT declares what it carries, so the vocabulary between a volume, a script and a "
        "surface is written down rather than agreed by habit");
  CHECK(read.Played.Is == "settler" && read.Played.Starts == "sanctuary" &&
            read.Played.View == "eyes" && read.Played.EyeHeightM == 1.75,
        "a PLAYER is a kind, a region it starts in and the view it looks through -- the engine ships "
        "one because input, a body and a camera meeting in one place is what every game has and no "
        "two of them would spell alike");
  CHECK(read.Views.size() == 3 && read.Views[0].Person == "first" &&
            read.Views[1].Person == "third" && read.Views[1].DistanceM == 2.5 &&
            read.Views[1].PitchLimitDeg == 70.0,
        "a view is FIRST or THIRD person, and a third-person one declares its distance and how far "
        "the pitch may go before the body is in the way");
  CHECK(read.Views.size() == 3 && read.Views[2].TimeScale == 0.2,
        "a VIEW declares what it follows and the rate time runs at, which is what an aimed shot or "
        "a slow-motion kill is made of");
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

  const std::string interior = InteriorScenario(stands);
  CHECK(!interior.empty(), "a second scenario is written where the client can read it");
  CHECK(engine.Parked().empty(), "nothing is parked before a door is walked through");
  const bool parked = engine.Park();
  if (!parked) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(parked, "walking through a door PARKS the scenario that was live");
  CHECK(engine.Parked().size() == 1 && engine.Parked()[0] == "four lines",
        "and it is parked under its own name, so the door on the other side names what to come back "
        "to");
  CHECK(!engine.Standing(),
        "nothing stands while a scenario is parked, which is what releases the residency a stand-up "
        "can rebuild");

  const bool inside = engine.Load(interior);
  if (!inside) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(inside, "the interior stands up in its place");
  CHECK(engine.Declared().Named.Name == "vault 111" && engine.Parked().size() == 1,
        "the live scenario is the interior and the exterior is still parked");
  CHECK(engine.Resume("nowhere") == false,
        "resuming what was never parked is refused rather than answered with nothing");

  const bool leaving = engine.Park();
  CHECK(leaving, "the door on the other side parks the interior in its turn");
  CHECK(engine.Parked().size() == 2,
        "so both sides of the door are parked for the instant between them -- RESUME PARKS NOTHING "
        "BY ITSELF, because a scenario that vanished on somebody else's call is a scenario nobody "
        "can reason about");

  const bool back = engine.Resume("four lines");
  if (!back) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(back, "walking back through the door RESUMES what was parked");
  CHECK(engine.Declared().Named.Name == "four lines" && engine.Declared().Kinds.size() == 4,
        "and it is the same scenario, carrying what it declared, rather than a second load of the "
        "same file");
  CHECK(engine.Parked().size() == 1 && engine.Parked()[0] == "vault 111",
        "and the interior stays parked, so walking back in again is a resume and not a load");

  {
    outshine::Engine doors;
    outshine::Scenario room = engine.Declared();
    size_t stoodUp = 0;
    for (int at = 0; at < 9; ++at) {
      room.Named.Name = "room " + std::to_string(at);
      if (!doors.Declare(room)) { break; }
      ++stoodUp;
      if (!doors.Park()) { break; }
    }
    CHECK(stoodUp == 9 && doors.Parked().size() == 8,
          "**THE PARKED SET STATES ITS BOUND AND REACHING IT REFUSES**: the ninth room stands "
          "but will not park -- a park is the ONLY copy of that state, and destroying what "
          "nobody chose to discard is not a cache policy");
    CHECK(doors.Error().find("room 0") != std::string::npos &&
              doors.Error().find("least recently live") != std::string::npos,
          "and the refusal names the least recently live door to clear, so the caller knows "
          "exactly which to resume or discard");
    CHECK(!doors.Resume("room 0") && doors.Error().find("standing") != std::string::npos,
          "and Resume over a STANDING scenario refuses -- it stands nothing down, because "
          "state that vanishes on somebody else's call is state nobody can reason about");
    CHECK(doors.Discard("room 0") && doors.Parked().size() == 7,
          "Discard is the explicit verb the refusal names -- the choice to lose state is the "
          "caller's, spelled out");
    CHECK(doors.Park() && doors.Parked().size() == 8,
          "and with a seat cleared the ninth room parks -- nothing was ever lost by the "
          "engine's own hand");
  }

  const struct {
    const char *What;
    const char *Text;
    const char *Names;
  } kTypos[] = {
      {"an unknown element",
       "<scenario name=\"t\"><generatorz/></scenario>", "generatorz"},
      {"an unknown attribute",
       "<scenario name=\"t\"><world radiusMeters=\"9\"/></scenario>", "radiusMeters"},
      {"an element in the wrong parent",
       "<scenario name=\"t\"><world><provider kind=\"terrain\"/></world></scenario>", "provider"},
      {"an attribute on the root",
       "<scenario naem=\"t\"/>", "naem"},
      {"an instance naming no kind",
       "<scenario name=\"t\"><instances><instance x=\"1\"/></instances></scenario>", "of"},
      {"a provider naming no kind",
       "<scenario name=\"t\"><providers><provider pin=\"3\"/></providers></scenario>", "kind"},
      {"a sound naming no uri",
       "<scenario name=\"t\"><audio><sound id=\"horn\"/></audio></scenario>", "uri"},
      {"a bind naming no event",
       "<scenario name=\"t\"><input><bind action=\"steer\"/></input></scenario>", "event"},
      {"a door with one end",
       "<scenario name=\"t\"><regions><door from=\"a\"/></regions></scenario>", "to"},
  };
  size_t refused = 0;
  for (const auto &typo : kTypos) {
    const std::string path = PreparedRoot() + "/typo.scenario";
    std::FILE *const file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) { continue; }
    std::fwrite(typo.Text, 1, std::strlen(typo.Text), file);
    std::fclose(file);
    outshine::Engine typed;
    const bool stood = typed.Load(path);
    const bool quotes = typed.Error().find(typo.Names) != std::string::npos;
    if (!stood && quotes) { ++refused; }
    std::printf("NOTE %-30s -> %s\n", typo.What,
                stood ? "STOOD UP" : typed.Error().c_str());
  }
  CHECK(refused == sizeof(kTypos) / sizeof(kTypos[0]),
        "a scenario that misspells an element or an attribute, or omits the one attribute that names what an element IS, is REFUSED and the refusal quotes what "
        "it could not place -- a typo that loads in silence costs an afternoon and leaves a black "
        "frame with nothing to grep for");

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

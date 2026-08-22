#include <cstdio>
#include <string>

#include <outshine/Outshine.h>

#include "Check.h"

namespace {

bool Planted(const std::string &at, const char *text) {
  std::FILE *const file = std::fopen(at.c_str(), "wb");
  if (file == nullptr) { return false; }
  std::fputs(text, file);
  std::fclose(file);
  return true;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string base = PlantedPath("layered.scenario");
  const std::string mod = PlantedPath("winter-mod.scenario");
  CHECK(Planted(mod, "<scenario name=\"winter\">"
                     "<kinds><kind name=\"mug\"><has name=\"volumeL\" value=\"0.7\"/></kind>"
                     "<kind name=\"lantern\"/></kinds>"
                     "<instances><instance of=\"mug\" id=\"cup\"/></instances></scenario>") &&
            Planted(base, std::string("<scenario name=\"game\" active=\"winter\">"
                           "<kinds><kind name=\"mug\">"
                           "<has name=\"volumeL\" value=\"0.5\"/></kind></kinds>"
                           "<layer id=\"winter\" path=\"winter-mod.scenario\" set=\"winter\"/>"
                           "<layer id=\"summer\" path=\"no-such.scenario\" set=\"summer\"/>"
                           "</scenario>")
                              .c_str()),
        "a base scenario and a mod layer are planted beside each other in the nest");

  outshine::Engine engine;
  const bool read = engine.Read(base);
  if (!read) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(read, "**A MOD IS A LAYER THE SCENARIO ORDERS**: the base reads, its active layer "
              "loads from beside it through the same reader, and the inactive summer layer "
              "is never even opened -- its file does not exist and nothing refused");
  if (!read) { return Report(); }

  CHECK(engine.Declared().Kinds.size() == 2 &&
            engine.Declared().Kinds[0].Attributes[0].Value == "0.7",
        "the mod's mug overrode the base's and the lantern was added -- by id, per row");
  bool traced = false, skipped = false;
  for (const std::string &row : engine.Carried()) {
    if (row.find("overrode kind 'mug'") != std::string::npos) { traced = true; }
    if (row.find("'summer' is inactive") != std::string::npos) { skipped = true; }
  }
  CHECK(traced && skipped,
        "and Carried publishes both what overrode what and which layer the change set left "
        "inactive");

  CHECK(engine.Assemble(), "the layered declaration assembles through the one door");
  const outshine::Entity cup = engine.Stood().InstanceNamed("cup");
  const outshine::Traits *held = engine.Resolved().Get(cup);
  CHECK(held != nullptr &&
            held->Named(engine.Stood().TraitKey("volumeL")) != nullptr &&
            *held->Named(engine.Stood().TraitKey("volumeL")) == 0.7,
        "**AND THE TRAITS READ BACK THROUGH THE SAME DOOR THEY STOOD THROUGH** -- the mod's "
        "0.7 L, resolved once, reachable from a client that includes nothing but outshine/");

  Covers("III.6.1 the layer door is files: a scenario's layers load relative to it, through "
         "the one reader, selected by the declaration's active set, traced on Carried "
         "(board:1493)");
  return Report();
}

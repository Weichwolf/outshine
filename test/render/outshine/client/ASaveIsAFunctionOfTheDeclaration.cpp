#include <cstdio>
#include <string>

#include <outshine/Outshine.h>

#include "Check.h"

namespace {

std::string Planted(const char *name, const char *text) {
  const std::string at = outshine::Test::PlantedPath(name);
  std::FILE *const file = std::fopen(at.c_str(), "wb");
  if (file == nullptr) { return std::string(); }
  std::fputs(text, file);
  std::fclose(file);
  return at;
}

std::string Slurp(const std::string &path) {
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) { return std::string(); }
  std::string text;
  char block[4096];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    text.append(block, read);
  }
  std::fclose(file);
  return text;
}

const char *kWorld =
    "<scenario name=\"vault\" version=\"3\">"
    "<kinds><kind name=\"mug\"><has name=\"fillL\" value=\"0.5\"/></kind></kinds>"
    "<instances><instance of=\"mug\" id=\"cup\"/></instances>"
    "<state><persist what=\"cup.fillL\"/></state></scenario>";

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string world = Planted("savable.scenario", kWorld);
  outshine::Engine engine;
  CHECK(engine.Read(world) && engine.Assemble(), "a savable scenario stands");

  const std::string save = PlantedPath("vault.save");
  CHECK(engine.Save(save), "**ONLY WHAT THE SCENARIO DECLARED PERSISTS** -- the save writes "
                           "the one declared trait and nothing else");
  const std::string first = Slurp(save);
  CHECK(first.find("outshine-save 1 vault 3") == 0,
        "**A SAVE NAMES THE SCENARIO AND ITS VERSION** in its first line");
  CHECK(engine.Save(save) && Slurp(save) == first,
        "**A SAVE IS DETERMINISTIC**: two saves of one state are the same bytes, so a diff "
        "means the STATE moved");

  {
    outshine::Engine second;
    CHECK(second.Read(world) && second.Assemble(),
          "**LOADING IS STANDING UP AND THEN APPLYING** -- the same one arrival route");
    CHECK(second.Restore(save), "the save applies through the same traits column it was "
                                "read from");
    const double *fill = second.Resolved().Get(second.Stood().InstanceNamed("cup"))
                             ->Named(second.Stood().TraitKey("fillL"));
    CHECK(fill != nullptr && *fill == 0.5, "and the value is back to the bit");
  }
  {
    outshine::Engine wrong;
    const std::string other = Planted(
        "other.scenario",
        "<scenario name=\"vault\" version=\"4\"><kinds><kind name=\"mug\">"
        "<has name=\"fillL\" value=\"0.5\"/></kind></kinds>"
        "<instances><instance of=\"mug\" id=\"cup\"/></instances>"
        "<state><persist what=\"cup.fillL\"/></state></scenario>");
    CHECK(wrong.Read(other) && wrong.Assemble() && !wrong.Restore(save) &&
              wrong.Error().find("vault 3") != std::string::npos &&
              wrong.Error().find("vault 4") != std::string::npos,
          "a save from a version this engine does not stand refuses QUOTING BOTH");
  }
  {
    outshine::Engine empty;
    const std::string none = Planted("stateless.scenario",
                                     "<scenario name=\"still\"><kinds><kind name=\"m\"/>"
                                     "</kinds></scenario>");
    CHECK(empty.Read(none) && empty.Assemble() && !empty.Save(save) &&
              empty.Error().find("nothing to persist") != std::string::npos,
          "a scenario that declares nothing saves nothing -- and says so instead of writing "
          "an empty promise");
  }

  Covers("III.9 what a scenario declared as state survives the process: a save is a "
         "deterministic function of the declaration, named and versioned, applied after the "
         "one stand-up route, bounded (board:1492)");
  return Report();
}

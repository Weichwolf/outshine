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
    // the save is edited to a NON-default value first, so an empty Restore cannot pass
    std::string tampered = Slurp(save);
    const size_t five = tampered.find("0.5");
    CHECK(five != std::string::npos, "the saved value is in the text");
    tampered.replace(five, 3, "0.75");
    const std::string edited = Planted("vault-edited.save", tampered.c_str());
    CHECK(second.Restore(edited), "the edited save applies through the same traits column "
                                  "it was read from");
    const double *fill = second.Resolved().Get(second.Stood().InstanceNamed("cup"))
                             ->Named(second.Stood().TraitKey("fillL"));
    CHECK(fill != nullptr && *fill == 0.75,
          "and the LANDED value differs from the declaration's default -- a Restore that "
          "does nothing cannot pass this");
  }
  {
    // a save whose LAST line is broken must leave the scene untouched -- never half-applied
    std::string broken = Slurp(save);
    broken += "cup.noSuchTrait 9\n";
    const std::string half = Planted("vault-broken.save", broken.c_str());
    outshine::Engine untouched;
    CHECK(untouched.Read(world) && untouched.Assemble() && !untouched.Restore(half),
          "a save with one bad line refuses");
    const double *kept = untouched.Resolved().Get(untouched.Stood().InstanceNamed("cup"))
                             ->Named(untouched.Stood().TraitKey("fillL"));
    CHECK(kept != nullptr && *kept == 0.5,
          "**AND NOTHING WAS HALF-APPLIED**: the good first line did not land before the bad "
          "last line refused -- validation completes before one value moves");
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

  {
    // the APPLY-stage refusal: a full sixteen-trait holder meets a key that IS interned
    // globally but has no seat on it -- the first (legal, tampered) line must NOT land
    const std::string fullWorld = Planted(
        "full.scenario",
        "<scenario name=\"full\" version=\"1\"><kinds><kind name=\"box\"><has name=\"t01\" value=\"0.5\"/><has name=\"t02\" value=\"0.5\"/><has name=\"t03\" value=\"0.5\"/><has name=\"t04\" value=\"0.5\"/><has name=\"t05\" value=\"0.5\"/><has name=\"t06\" value=\"0.5\"/><has name=\"t07\" value=\"0.5\"/><has name=\"t08\" value=\"0.5\"/><has name=\"t09\" value=\"0.5\"/><has name=\"t10\" value=\"0.5\"/><has name=\"t11\" value=\"0.5\"/><has name=\"t12\" value=\"0.5\"/><has name=\"t13\" value=\"0.5\"/><has name=\"t14\" value=\"0.5\"/><has name=\"t15\" value=\"0.5\"/><has name=\"t16\" value=\"0.5\"/></kind>"
        "<kind name=\"spare\"><has name=\"extra\" value=\"1\"/></kind></kinds>"
        "<instances><instance of=\"box\" id=\"crate\"/></instances>"
        "<state><persist what=\"crate.t01\"/></state></scenario>");
    outshine::Engine packed;
    CHECK(packed.Read(fullWorld) && packed.Assemble(), "a sixteen-trait holder stands");
    const std::string fullSave = PlantedPath("full.save");
    CHECK(packed.Save(fullSave), "and saves its one declared trait");
    std::string sabotaged = Slurp(fullSave);
    const size_t half = sabotaged.find("0.5");
    CHECK(half != std::string::npos, "the saved value is in the text");
    sabotaged.replace(half, 3, "0.9");
    sabotaged += "crate.extra 1\n";
    const std::string trap = Planted("full-sabotaged.save", sabotaged.c_str());
    CHECK(!packed.Restore(trap) &&
              packed.Error().find("never declared") != std::string::npos,
          "a globally interned key with no seat on the holder refuses in the dry run -- "
          "Save refuses to write a missing value and Restore refuses to GRAFT one, the "
          "same voice from the other side (board:1708)");
    const double *held = packed.Resolved().Get(packed.Stood().InstanceNamed("crate"))
                             ->Named(packed.Stood().TraitKey("t01"));
    CHECK(held != nullptr && *held == 0.5,
          "**AND THE LEGAL FIRST LINE DID NOT LAND**: the apply-stage refusal keeps the "
          "whole-or-nothing contract, not just the validation-stage one (board:1702)");
  }

  {
    // disk input is a boundary: a tail behind the number and a hand-spelled inf both
    // refuse, because Save can write neither -- the line is an edit, not a save
    outshine::Engine picky;
    CHECK(picky.Read(world) && picky.Assemble(), "the boundary engine stands");
    const std::string clean = PlantedPath("vault-clean.save");
    CHECK(picky.Save(clean), "and saves");
    std::string tailed = Slurp(clean);
    const size_t five = tailed.find("0.5");
    CHECK(five != std::string::npos, "the value is in the text");
    tailed.replace(five, 3, "1.5garbage");
    CHECK(!picky.Restore(Planted("vault-tailed.save", tailed.c_str())) &&
              picky.Error().find("garbage") != std::string::npos,
          "**A VALUE WITH A TAIL REFUSES NAMING THE LINE** -- 1.5garbage never lands as 1.5 "
          "(board:1708)");
    std::string poisoned = Slurp(clean);
    poisoned.replace(poisoned.find("0.5"), 3, "inf");
    CHECK(!picky.Restore(Planted("vault-inf.save", poisoned.c_str())),
          "and a hand-spelled inf refuses -- Save writes only finite numbers, so Restore "
          "accepts only finite numbers");
    const double *kept = picky.Resolved().Get(picky.Stood().InstanceNamed("cup"))
                             ->Named(picky.Stood().TraitKey("fillL"));
    CHECK(kept != nullptr && *kept == 0.5, "and both refusals left the column untouched");
  }

  Covers("III.9 what a scenario declared as state survives the process: a save is a "
         "deterministic function of the declaration, named and versioned, applied after the "
         "one stand-up route, bounded (board:1492)");
  return Report();
}

#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Run;

namespace {

// A seeded copy pins ROOT to the calling directory -- the copy lives in the checkout-keyed
// nest, so a neighbour cannot overwrite it between the write and the sh -- and the detectors
// are byte-identical to the runner's own.
//
// WHERE THE SEED GOES IS READ FROM THE DECLARATION, NEVER QUOTED. Every one of these controls
// used to name a literal path list -- "src/sim src/scene src/engine/Assembly.cpp" -- copied out
// of `LayerGroups` on the day it was written. The day somebody reordered that list the sed
// matched nothing, the copy came back byte-identical to the original, and the control stopped
// controlling. It did not go red: an unseeded copy passes its own audit, so the claim went
// GREEN with four detectors nobody had checked.
//
// So the sites are DERIVED from `LayerGroups` as it stands at this instant, and every seed is
// followed by a check that it took. A seed that did not take is reported as a STALE CONTROL by
// that name, because "the ghost detector did not fire" and "there was no ghost to detect" are
// different failures and only one of them is about the audit.
[[nodiscard]] std::string Seeded(const char *name, const std::string &patch) {
  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr) { return std::string(); }
  const std::string at = std::string(nest) + "/audit-control-" + name + ".sh";
  std::string ignored;
  (void)Run("sed -e 's|^ROOT=.*|ROOT=\"$PWD\"|' -e '/^LayerGroups()/,/^}/{" + patch +
                ";}' test/run.sh > " + at,
            ignored);
  return at;
}

[[nodiscard]] std::string OneWord(const std::string &of) {
  const size_t end = of.find_first_of(" \n\t");
  return end == std::string::npos ? of : of.substr(0, end);
}

// A unit named ON ITS OWN whose directory the declaration does NOT also list. Striking one that
// its directory covers changes nothing -- the glob still finds it -- so a control seeded there
// watches a closure that never opened.
[[nodiscard]] std::string OnlyNamedAlone() {
  std::string said;
  (void)Run("L=$(sed -n '/^LayerGroups()/,/^}/p' test/run.sh"
            " | grep -o 'src/[a-zA-Z0-9/._-]*' | sort -u);"
            " for c in $(printf '%s\\n' \"$L\" | grep '\\.cpp$'); do"
            " d=$(dirname \"$c\");"
            " printf '%s\\n' \"$L\" | grep -qx \"$d\" || { printf '%s\\n' \"$c\"; break; };"
            " done",
            said);
  return said;
}

// The declaration as it stands NOW: every source-group token `LayerGroups` names, one per line.
[[nodiscard]] std::string Listed(const char *matching) {
  std::string said;
  (void)Run(std::string("sed -n '/^LayerGroups()/,/^}/p' test/run.sh"
                        " | grep -o 'src/[a-zA-Z0-9/._-]*' | sort -u | ") +
                matching + " | head -1",
            said);
  return OneWord(said);
}

// A copy the seed could not be applied to is EMPTY, and an empty file contains no defect to
// find -- which is how a control that is not running looks exactly like a control that passed.
// Every stale-control check asks first whether the copy is still a runner.
[[nodiscard]] bool StillARunner(const std::string &copy) {
  return copy.size() > 1000 && copy.find("LayerGroups()") != std::string::npos;
}

// The seed is applied inside `LayerGroups` and nowhere else, so the check that it took reads
// the same region. A path also spelled in an include set or an error message is not a listing.
[[nodiscard]] std::string Declaration(const std::string &of) {
  std::string said;
  (void)Run("sed -n '/^LayerGroups()/,/^}/p' " + of, said);
  return said;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  CHECK(std::getenv("OUTSHINE_NEST") != nullptr,
        "run.sh exports the checkout-keyed nest the seeded copies are written into");
  if (std::getenv("OUTSHINE_NEST") == nullptr) { return Report(); }

  std::string listing;
  const int listingVerdict = Run("sh test/run.sh --audit 2>&1", listing);
  std::printf("%s", listing.c_str());
  CHECK(listingVerdict == 0 && listing.find("AUDIT clean") != std::string::npos,
        "**EVERY DECLARED SUITE LISTS EACH SOURCE ONCE, AND EVERY SOURCE HAS A SUITE**: a file "
        "listed beside its own directory links twice; a file in no suite's closure never links "
        "at all -- both shipped in one week and both were invisible to a gate that compiles but "
        "never links the named-only suites (board:1641)");

  const std::string aDirectory = Listed("grep -v '\\.cpp$'");
  std::string aUnitSaid;
  (void)Run("find " + aDirectory + " -maxdepth 1 -name '*.cpp' | sort | head -1", aUnitSaid);
  const std::string aUnit = OneWord(aUnitSaid);
  std::printf("SEED SITES  directory %s   unit under it %s\n", aDirectory.c_str(), aUnit.c_str());
  CHECK(!aDirectory.empty() && !aUnit.empty(),
        "STALE CONTROL: the declaration names no source directory holding a unit, so there is "
        "nowhere to seed a duplicate and the controls below would pass on an unseeded copy");
  if (aDirectory.empty() || aUnit.empty()) { return Report(); }

  const std::string doubled =
      Seeded("listing", "s|" + aDirectory + "|" + aDirectory + " " + aUnit + "|g");
  std::string copy;
  (void)Run("cat " + doubled, copy);
  const std::string doubledList = Declaration(doubled);
  CHECK(StillARunner(copy) && doubledList.find(aDirectory + " " + aUnit) != std::string::npos,
        "STALE CONTROL: the duplicate seed did not take. A copy identical to the original passes "
        "its own audit and an EMPTY copy contains no defect to find, so either way the detector "
        "below would report success having detected nothing");
  std::string seededSaid;
  const int seededVerdict = Run("sh " + doubled + " --audit 2>&1", seededSaid);
  CHECK(seededVerdict != 0 && seededSaid.find("lists twice") != std::string::npos &&
            seededSaid.find(aUnit) != std::string::npos,
        "**THE DUPLICATE DETECTOR DETECTS**: a unit listed beside the directory that already "
        "holds it links twice, the seeded copy flips the verdict, and the message names the "
        "very unit that was doubled");

  std::string closed;
  const int closedVerdict = Run("sh test/run.sh --audit-link 2>&1", closed);
  std::printf("%s", closed.c_str());
  CHECK(closedVerdict == 0 && closed.find("AUDIT closed") != std::string::npos,
        "**AND EVERY DECLARED SUITE RESOLVES ITS OWN SYMBOLS FROM ITS OWN OBJECTS**: the gate "
        "compiles every source but only the named runs ever LINK the named-only suites, so a "
        "sources list that lost a unit stayed invisible until a sporadic five-minute run "
        "refused. The closure walks each declared suite's object set with nm (board:1641)");

  const std::string aNamedUnit = OneWord(OnlyNamedAlone());
  std::printf("SEED SITE   unit named on its own %s\n", aNamedUnit.c_str());
  CHECK(!aNamedUnit.empty(),
        "STALE CONTROL: the declaration names no source on its own whose directory it does not "
        "also list, so striking one would leave the glob to find it and the closure would never "
        "open");
  if (aNamedUnit.empty()) { return Report(); }

  const std::string lost = Seeded("link", "s| *" + aNamedUnit + "||g");
  std::string struck;
  (void)Run("cat " + lost, struck);
  const std::string lostList = Declaration(lost);
  CHECK(StillARunner(struck) && lostList.find(aNamedUnit) == std::string::npos,
        "STALE CONTROL: the strike did not take, so the closure detector below would be asked "
        "to find a hole that was never made");
  std::string missing;
  const int missingVerdict = Run("sh " + lost + " --audit-link 2>&1", missing);
  CHECK(missingVerdict != 0 && missing.find("cannot resolve") != std::string::npos,
        "**AND THE CLOSURE DETECTOR DETECTS**: with a unit struck from the declaration the audit "
        "flips and names a symbol it can no longer resolve -- the silent absence that filed the "
        "item in the first place");

  const std::string ghost = Seeded(
      "ghost", "s|" + aDirectory + "|" + aDirectory + "/NoSuchUnit.cpp " + aDirectory + "|g");
  std::string conjured;
  (void)Run("cat " + ghost, conjured);
  const std::string ghostList = Declaration(ghost);
  CHECK(StillARunner(conjured) &&
            ghostList.find(aDirectory + "/NoSuchUnit.cpp") != std::string::npos,
        "STALE CONTROL: the ghost seed did not take, so the refusal below would mean the copy "
        "was clean and not that ghosts are caught");
  std::string haunted;
  const int ghostVerdict = Run("sh " + ghost + " --audit-link 2>&1", haunted);
  CHECK(ghostVerdict != 0 && haunted.find("ghost in the listing") != std::string::npos &&
            haunted.find("NoSuchUnit.cpp") != std::string::npos,
        "and a declaration naming a source that does not EXIST refuses as a ghost, which is how "
        "a deleted unit sat in the listing unnoticed (board:1658)");

  Covers("IV.27 the build declaration audits itself in the fast gate: every source is listed "
         "once and by some suite, every declared suite's object set is closed over its "
         "undefined outshine symbols, ghosts refuse -- and all four detectors are "
         "negative-controlled against seeded defects on every run (board:1641, 1658)");
  return Report();
}

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
[[nodiscard]] std::string Seeded(const char *name, const std::string &patch) {
  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr) { return std::string(); }
  const std::string at = std::string(nest) + "/audit-control-" + name + ".sh";
  std::string ignored;
  (void)Run("sed -e 's|^ROOT=.*|ROOT=\"$PWD\"|' -e '" + patch + "' test/run.sh > " + at, ignored);
  return at;
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

  const std::string doubled =
      Seeded("listing", "s|src/sim src/scene src/engine/Assembly.cpp"
                        "|src/sim src/scene src/scene/Store.cpp src/engine/Assembly.cpp|;"
                        "s|src/compositor src/engine/Live.cpp|src/engine/Live.cpp|");
  std::string copy;
  (void)Run("cat " + doubled, copy);
  CHECK(copy.find("src/scene src/scene/Store.cpp") != std::string::npos &&
            copy.find("src/compositor src/engine/Live.cpp") == std::string::npos,
        "both seeds took -- a duplicate beside src/scene, and the compositor struck from the "
        "only suite that lists it");
  std::string seededSaid;
  const int seededVerdict = Run("sh " + doubled + " --audit 2>&1", seededSaid);
  CHECK(seededVerdict != 0 && seededSaid.find("lists twice") != std::string::npos &&
            seededSaid.find("no suite links src/compositor/") != std::string::npos &&
            seededSaid.find("reach no suite, and the declaration says") != std::string::npos,
        "**AND BOTH LISTING DETECTORS DETECT**: one dually-seeded copy flips the verdict and "
        "names BOTH defects, because the audit collects every defect before it judges");

  std::string closed;
  const int closedVerdict = Run("sh test/run.sh --audit-link 2>&1", closed);
  std::printf("%s", closed.c_str());
  CHECK(closedVerdict == 0 && closed.find("AUDIT closed") != std::string::npos,
        "**AND EVERY DECLARED SUITE RESOLVES ITS OWN SYMBOLS FROM ITS OWN OBJECTS**: the gate "
        "compiles every source but only the named runs ever LINK the named-only suites, so a "
        "sources list that lost a unit stayed invisible until a sporadic five-minute run "
        "refused. The closure walks each declared suite's object set with nm (board:1641)");

  const std::string lost =
      Seeded("link", "s|src/scenario/Views.cpp src/scenario/InputMap.cpp"
                     "|src/scenario/InputMap.cpp|g");
  std::string struck;
  (void)Run("cat " + lost, struck);
  CHECK(struck.find("src/scenario/Views.cpp src/scenario/InputMap.cpp") == std::string::npos,
        "the seed took -- the view book left every suite's closure in the copy");
  std::string missing;
  const int missingVerdict = Run("sh " + lost + " --audit-link 2>&1", missing);
  CHECK(missingVerdict != 0 && missing.find("cannot resolve") != std::string::npos &&
            missing.find("ViewBook") != std::string::npos,
        "**AND THE CLOSURE DETECTOR DETECTS**: with the view book struck from the declaration "
        "the audit flips and names the very symbol whose silent absence filed the item");

  const std::string ghost =
      Seeded("ghost", "s|src/engine/InputPump.cpp src/engine/Assembly.cpp"
                      "|src/engine/NoSuchUnit.cpp src/engine/InputPump.cpp "
                      "src/engine/Assembly.cpp|g");
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

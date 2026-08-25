#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;
using outshine::Test::Run;

namespace {

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string said;
  const int verdict = Run("sh test/run.sh --audit-link 2>&1", said);
  std::printf("%s", said.c_str());
  CHECK(verdict == 0 && said.find("AUDIT closed") != std::string::npos,
        "**EVERY DECLARED SUITE RESOLVES ITS OWN SYMBOLS FROM ITS OWN OBJECTS.** The gate "
        "compiles every source but only the named runs ever LINK the named-only suites -- so a "
        "sources list that lost a unit was invisible until a sporadic five-minute run refused "
        "(board:1641's world shape). The closure walks every declared suite's object set with "
        "nm and refuses an outshine symbol nothing in the set defines");

  const char *nest = std::getenv("OUTSHINE_NEST");
  CHECK(nest != nullptr, "run.sh exports the checkout-keyed nest the control writes into");
  if (nest == nullptr) { return Report(); }
  const std::string at = std::string(nest) + "/audit-link-control.sh";
  std::string ignored;
  (void)Run("sed -e 's|^ROOT=.*|ROOT=\"$PWD\"|' "
            "-e 's|src/ground src/actor/path/Wayfinding.cpp src/clients/Sim.cpp"
            "|src/ground src/clients/Sim.cpp|' test/run.sh > " + at, ignored);
  std::string copy;
  (void)Run("cat " + at, copy);
  CHECK(copy.find("src/ground src/clients/Sim.cpp") != std::string::npos,
        "the seed took -- Wayfinding left the world suite's closure in the copy");
  std::string lost;
  const int lostVerdict = Run("sh " + at + " --audit-link render/outshine/world 2>&1", lost);
  CHECK(lostVerdict != 0 && lost.find("cannot resolve") != std::string::npos &&
            lost.find("__ZN8outshine4Path7Network") != std::string::npos,
        "**AND THE DETECTOR DETECTS**: with Wayfinding struck from the declaration, the audit "
        "flips and names the very symbol whose silent absence filed the item");

  const std::string ghost = std::string(nest) + "/audit-ghost-control.sh";
  (void)Run("sed -e 's|^ROOT=.*|ROOT=\"$PWD\"|' "
            "-e 's|src/ground src/actor/path/Wayfinding.cpp|src/ground src/actor/path/NoSuchUnit.cpp src/actor/path/Wayfinding.cpp|' test/run.sh > " + ghost, ignored);
  std::string haunted;
  const int ghostVerdict = Run("sh " + ghost + " --audit-link render/outshine/world 2>&1", haunted);
  CHECK(ghostVerdict != 0 && haunted.find("ghost in the listing") != std::string::npos &&
            haunted.find("NoSuchUnit.cpp") != std::string::npos,
        "and a declaration naming a source that does not EXIST refuses as a ghost -- the class "
        "SceneWeather.cpp sat in silently until the exact-object audit tripped over it");

  Covers("IV.28 the build declaration audits itself: beyond one listing per source, every "
         "declared suite's object set is closed over its undefined outshine symbols -- read "
         "from the EXACT set-stamped objects the declaration names, ghosts refused -- "
         "negative-controlled against a seeded loss and a seeded ghost on every run "
         "(board:1641, 1658)");
  return Report();
}

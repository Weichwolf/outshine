#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"

namespace {

int Run(const std::string &cmd, std::string &said) {
  std::FILE *const pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) { return -1; }
  char block[512];
  while (std::fgets(block, sizeof block, pipe) != nullptr) { said += block; }
  return pclose(pipe);
}

// A seeded copy pins ROOT to the calling directory (the copy lives in the temp dir, so its
// own $0 would point the audit at the wrong tree); the detectors are byte-identical.
// the copy lands inside the checkout-keyed nest run.sh exports (board:1650) -- a fixed
// name in the shared temp root is a neighbour's to overwrite between write and sh
std::string Seeded(const std::string &name, const std::string &patch) {
  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr) { return std::string(); }
  std::string at = std::string(nest) + "/audit-control-" + name + ".sh";
  std::string said;
  (void)Run("sed -e 's|^ROOT=.*|ROOT=\"$PWD\"|' -e '" + patch + "' test/run.sh > " + at, said);
  return at;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string said;
  const int verdict = Run("sh test/run.sh --audit 2>&1", said);
  std::printf("%s", said.c_str());
  CHECK(verdict == 0,
        "**EVERY DECLARED SUITE LISTS EACH SOURCE ONCE, AND EVERY SOURCE HAS A SUITE.** A file "
        "listed beside its own directory links twice; a file in no suite's closure never links "
        "at all -- both shipped this week and both were invisible to a gate that compiles but "
        "never links the named suites. The audit is the declaration's own consistency check");
  CHECK(said.find("AUDIT clean") != std::string::npos, "and it says so in words");

  const std::string seeded = Seeded(
      "both", "s|\"src/scene\" ;;|\"src/scene src/scene/Store.cpp\" ;;|;"
              "s|Wayfinding.cpp src/clients/Sim.cpp src/clients/|Wayfinding.cpp src/clients/|");
  std::string copy;
  (void)Run("cat " + seeded, copy);
  CHECK(copy.find("src/scene src/scene/Store.cpp") != std::string::npos &&
            copy.find("Wayfinding.cpp src/clients/Sim.cpp") == std::string::npos,
        "both seeds took -- a duplicate beside src/scene, and Sim.cpp struck from the one "
        "closure that compiles it");
  std::string seededSaid;
  const int seededVerdict = Run("sh " + seeded + " --audit 2>&1", seededSaid);
  CHECK(seededVerdict != 0 && seededSaid.find("lists twice") != std::string::npos &&
            seededSaid.find("no suite compiles src/clients/Sim.cpp") != std::string::npos,
        "**AND THE DETECTORS DETECT**: one dually-seeded copy flips the verdict and names BOTH "
        "defects -- the audit collects every defect before it judges, so the control costs one "
        "run, not two (board:1641's negative control, no longer manual)");

  Covers("IV.7 the build declaration audits itself in the fast gate: duplicate listings and "
         "orphan sources refuse before any named suite has to discover them at link time, and "
         "both detectors are negative-controlled against a seeded duplicate and a seeded orphan "
         "on every run");
  return Report();
}

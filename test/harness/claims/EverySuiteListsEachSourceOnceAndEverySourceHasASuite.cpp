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
std::string Seeded(const std::string &name, const std::string &patch) {
  const char *tmp = std::getenv("TMPDIR");
  std::string at = (tmp != nullptr ? std::string(tmp) : std::string("/tmp"));
  if (!at.empty() && at.back() == '/') { at.pop_back(); }
  at += "/outshine-tests/audit-control-" + name + ".sh";
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

  const std::string dup =
      Seeded("dup", "s|\"src/scene\" ;;|\"src/scene src/scene/Store.cpp\" ;;|");
  std::string dupCopy;
  (void)Run("cat " + dup, dupCopy);
  CHECK(dupCopy.find("src/scene src/scene/Store.cpp") != std::string::npos,
        "the duplicate seed took -- unit/scene's group line still reads as the control expects");
  std::string dupSaid;
  const int dupVerdict = Run("sh " + dup + " --audit 2>&1", dupSaid);
  CHECK(dupVerdict != 0 && dupSaid.find("lists twice") != std::string::npos,
        "**AND THE DETECTOR DETECTS**: a seeded duplicate -- a file listed beside the directory "
        "that already expands to it -- flips the audit verdict and names the doubled listing "
        "(board:1641's negative control, no longer manual)");

  const std::string orphan = Seeded(
      "orphan", "s|Wayfinding.cpp src/clients/Sim.cpp src/clients/|Wayfinding.cpp src/clients/|");
  std::string orphanCopy;
  (void)Run("cat " + orphan, orphanCopy);
  CHECK(orphanCopy.find("Wayfinding.cpp src/clients/Sim.cpp") == std::string::npos,
        "the orphan seed took -- Sim.cpp left the one closure that compiles it in the copy");
  std::string orphanSaid;
  const int orphanVerdict = Run("sh " + orphan + " --audit 2>&1", orphanSaid);
  CHECK(orphanVerdict != 0 &&
            orphanSaid.find("no suite compiles src/clients/Sim.cpp") != std::string::npos,
        "and a seeded orphan -- a source struck from every suite's closure -- flips the verdict "
        "and names the file no suite would compile");

  Covers("IV.7 the build declaration audits itself in the fast gate: duplicate listings and "
         "orphan sources refuse before any named suite has to discover them at link time, and "
         "both detectors are negative-controlled against a seeded duplicate and a seeded orphan "
         "on every run");
  return Report();
}

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

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  CHECK(nest != nullptr && *nest != 0,
        "run.sh exports the nest, so this test runs under a runner that HOLDS the lock");
  if (nest == nullptr) { return Report(); }

  // stripped of the inherited nest, a child is an independent second runner -- and the
  // parent gate holds the lock, so it must refuse naming a live pid
  std::string said;
  const int verdict = Run("env -u OUTSHINE_NEST sh test/run.sh --audit 2>&1", said);
  std::printf("NOTE the second runner was told: %s", said.c_str());
  CHECK(verdict != 0 && said.find("holds this checkout's nest") != std::string::npos,
        "**A SECOND RUNNER IN THE SAME CHECKOUT REFUSES, NAMING THE HOLDER** -- two gates in "
        "one nest read each other's half-written objects, and the false-red that filed "
        "board:1659 cannot recur (the claim carries its pid in ONE atomic noclobber step, "
        "board:1662)");

  // the nested form -- this very test's parent chain -- passes through on the inherited
  // nest, which the audits running inside every gate prove on every run
  std::string nested;
  const int nestedVerdict = Run("sh test/run.sh --audit 2>&1", nested);
  CHECK(nestedVerdict == 0 && nested.find("AUDIT clean") != std::string::npos,
        "and a NESTED invocation on the inherited nest passes through the lock -- the gate's "
        "own inner audits keep working under the parent's claim");

  Covers("IV.26 one runner per nest: the lock claim and the claimant's identity are one "
         "atomic step, a second runner refuses naming the live holder, a stale claim breaks "
         "and exactly one breaker wins, and nested invocations pass through (board:1659, "
         "1662)");
  return Report();
}

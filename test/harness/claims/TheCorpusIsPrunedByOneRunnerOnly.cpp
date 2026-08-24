#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "Check.h"

namespace {

[[nodiscard]] int Run(const std::string &command, std::string &said) {
  std::FILE *const pipe = popen(command.c_str(), "r");
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
  CHECK(nest != nullptr && *nest != 0, "this test runs under a runner that holds the nest");
  if (nest == nullptr) { return Report(); }

  const char *tmp = std::getenv("TMPDIR");
  const std::string prepared =
      std::string(tmp == nullptr ? "/tmp/" : tmp) + "outshine-prepared";
  const std::string lock = prepared + ".lock";

  // board:1789: the nest carries a per-checkout identity and the corpus does not, so two
  // runners in two checkouts share one directory -- and pruning it is a DELETE. Sharing the
  // bytes is worth keeping; sharing the right to remove them is not. A runner that does not
  // hold the corpus claim reads it and never prunes.
  const bool held = std::filesystem::exists(lock);
  std::printf("NOTE the corpus lock stands: %s\n", held ? "yes" : "no");
  CHECK(held,
        "**A RUNNER THAT PRUNES THE CORPUS HOLDS ITS CLAIM**: this very run is pruning, so "
        "the claim it took must be on disk (board:1789)");

  std::string mine;
  {
    std::ifstream reading(lock);
    reading >> mine;
  }
  std::printf("NOTE the claim names pid %s\n", mine.c_str());
  CHECK(!mine.empty() && std::atol(mine.c_str()) > 0,
        "and it names the pid that took it, so a stale claim can be told from a live one");

  // a second runner, on the inherited nest so it passes the nest lock, must find the corpus
  // claim held and say it will not prune.
  // A second full runner is the honest experiment and it costs more than the fast gate has:
  // the only suites that actually prune are the corpus ones, and the cheapest of them
  // (harness/render/outshine/grown, 21 cases) runs past the 120 s bound when nested. So the
  // guard is asked DIRECTLY, the way board:1765 made the corpus question askable.
  //
  // This test runs UNDER a runner that holds the claim, so a child asking the question is
  // exactly the second-runner case: it must decline. Removing the claim first is the control.
  std::string underAHolder;
  const int heldVerdict = Run("sh test/run.sh --would-prune 2>&1", underAHolder);
  std::printf("NOTE a child under this runner: %s", underAHolder.c_str());
  CHECK(heldVerdict == 0, "a runner meeting a live foreign claim answers rather than dying");
  CHECK(underAHolder.find("would NOT prune") != std::string::npos,
        "**A RUNNER THAT DOES NOT HOLD THE CORPUS CLAIM REMOVES NOTHING**: eviction is "
        "serialised to the holder, so one runner's working set cannot vanish under it while "
        "another scores a case (board:1789)");

  const std::string parked = prepared + ".lock.parked";
  std::error_code why;
  std::filesystem::rename(lock, parked, why);
  std::string withNoHolder;
  const int freeVerdict = Run("sh test/run.sh --would-prune 2>&1", withNoHolder);
  std::filesystem::remove(lock, why);
  std::filesystem::rename(parked, lock, why);

  std::printf("NOTE with no claim standing: %s", withNoHolder.c_str());
  CHECK(freeVerdict == 0 && withNoHolder.find("WOULD prune") != std::string::npos,
        "and with no claim standing the same runner WOULD prune -- so the decline above is "
        "the claim's doing and not the runner's habit");
  CHECK(std::filesystem::exists(lock), "and this run's own claim is put back as it was");

  Covers("IV.15 the shared corpus is pruned by the runner holding its claim and by no other, "
         "so a second checkout reads the same bytes and removes none of them (board:1789)");
  return Report();
}

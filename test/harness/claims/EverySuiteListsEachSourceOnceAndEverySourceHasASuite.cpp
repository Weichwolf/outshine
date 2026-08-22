#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::FILE *const audit = popen("sh test/run.sh --audit 2>&1", "r");
  CHECK(audit != nullptr, "the runner's audit mode starts");
  std::string said;
  char block[512];
  while (audit != nullptr && std::fgets(block, sizeof block, audit) != nullptr) { said += block; }
  const int verdict = audit != nullptr ? pclose(audit) : -1;
  std::printf("%s", said.c_str());

  CHECK(verdict == 0,
        "**EVERY DECLARED SUITE LISTS EACH SOURCE ONCE, AND EVERY SOURCE HAS A SUITE.** A file "
        "listed beside its own directory links twice; a file in no suite's closure never links "
        "at all -- both shipped this week and both were invisible to a gate that compiles but "
        "never links the named suites. The audit is the declaration's own consistency check, "
        "negative-controlled against a seeded duplicate and a seeded orphan");
  CHECK(said.find("AUDIT clean") != std::string::npos, "and it says so in words");

  Covers("IV.7 the build declaration audits itself in the fast gate: duplicate listings and "
         "orphan sources refuse before any named suite has to discover them at link time");
  return Report();
}

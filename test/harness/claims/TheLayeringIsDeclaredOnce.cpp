#include <cstdio>
#include <string>
#include "Check.h"
#include "Shell.h"

int main() {
  using namespace outshine::Test;
  std::string said;
  const int result = Run("python3 test/scripts/layer-contract.py 2>&1", said);
  std::printf("%s", said.c_str());
  CHECK(result == 0, "declared tiers are acyclic and compiler include access stays within reaches");
  Covers("actual compilation database include directories against reaches; positive and negative "
         "fixtures reject undeclared access, cycles, missing tiers and prefix collisions");
  return Report();
}

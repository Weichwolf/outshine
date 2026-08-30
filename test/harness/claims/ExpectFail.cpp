#include "Check.h"

int main() {
  outshine::Test::Covers(
      "I.36 The harness's own red demonstrated by a test that is declared to fail");

  CHECK(1 + 1 == 2, "the reporter counts a claim that holds");
  CHECK_NEAR(2.0, 2.0, 1e-12, "of 1", "the reporter counts a near-claim that holds");

  CHECK_NEAR(11.0, 9.0, 1.0, "m", "the declared failure: 11 m is not 9 m to within 1 m");

  return outshine::Test::Report();
}

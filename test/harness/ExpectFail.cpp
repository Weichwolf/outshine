/* A HARNESS THAT HAS NEVER BEEN NON-ZERO IS A HARNESS NOBODY HAS TESTED. This test fails exactly one
 * claim, every time, on purpose; run.sh carries it in its expect-fail set with that count and inverts
 * it. So the red path is exercised on every run — and the day the reporter stops counting, or the
 * harness stops reading the exit code, this line turns red instead of green.
 *
 * The count is the point. An inversion on "non-zero" would also be satisfied by a crash, a build that
 * fell over or a test that failed twice, which is how a broken harness stays quiet. */
#include "Check.h"

int main() {
  outshine::Test::Covers("I.20 The harness's own red demonstrated by a test that is declared to fail");

  CHECK(1 + 1 == 2, "the reporter counts a claim that holds");
  CHECK_NEAR(2.0, 2.0, 1e-12, "of 1", "the reporter counts a near-claim that holds");

  /* THE DECLARED FAILURE. 2 m off a 10 m tolerance would pass; the tolerance is what makes this a
   * refusal, and the failure line below is what a reader gets to see on every green run. */
  CHECK_NEAR(11.0, 9.0, 1.0, "m", "the declared failure: 11 m is not 9 m to within 1 m");

  return outshine::Test::Report();
}

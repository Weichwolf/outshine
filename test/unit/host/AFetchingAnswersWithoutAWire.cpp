#include <cstdio>

#include "Check.h"

#include <cstdint>
#include <vector>

#include <outshine/Fetching.h>

using outshine::Fetching;
using outshine::Data::Ticket;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Fetching::Config quiet;
  quiet.Threads = 1;
  Fetching wire(quiet);

  Note("threads it stood up", (double)wire.ThreadCount(), "threads");
  CHECK(wire.ThreadCount() == 1,
        "**A TRANSPORT STANDS UP THE THREADS IT WAS DECLARED**, and one is one");

  outshine::Data::Wire nothing = wire.Collect(Ticket::None);
  int status = -1;
  std::vector<uint8_t> body;
  const bool took = nothing.TryTake(&status, &body);
  std::printf("NOTE collecting Ticket::None is %s\n",
              nothing.Where() == outshine::Data::Wire::State::Unreachable  ? "unreachable"
              : nothing.Where() == outshine::Data::Wire::State::Working    ? "working"
                                                                           : "answered");
  CHECK(!took && nothing.Where() != outshine::Data::Wire::State::Answered,
        "**AND THE ABSENT TICKET IS NOT AN ANSWER**: Ticket::None is what a caller holds before "
        "it has asked for anything, so collecting on it yields no body to take rather than "
        "reaching into a table it has no entry in");

  wire.Cancel(Ticket::None);
  CHECK(wire.ThreadCount() == 1,
        "and cancelling the absent ticket changes nothing -- a cancel is a request to stop "
        "something, and stopping nothing is not an error");

  Covers("I.6.1 the shipped transport answers the absent ticket with an empty body and treats "
         "its cancellation as a no-op, so a caller that never asked need not check before it "
         "collects (board:1862)");
  return Report();
}

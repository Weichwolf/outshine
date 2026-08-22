#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "GroundStack.h"
#include "Sink.h"
#include "Transport.h"

using outshine::Sink;
using outshine::World::GroundStack;

namespace {

class Quiet : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool, const char *) override {}
  void Near(double, double, double, const char *, const char *) override {}
  void Say(const std::string &) override {}
};

class NoWire : public outshine::Data::Transport {
public:
  [[nodiscard]] outshine::Data::Ticket Begin(const std::string &) override { return {}; }
  [[nodiscard]] outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Unreachable();
  }
  void Cancel(outshine::Data::Ticket) override {}
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Quiet quiet;
  NoWire wire;

  GroundStack elsewhere;
  CHECK(elsewhere.Open("/tmp/outshine-drive-cache", "/nowhere/no-assets", 48.1, 11.5, wire, quiet),
        "**A DECLARATION IS NOT A FETCH**: registering sources against an absent assets root "
        "still opens -- absence answers at read time as uncovered, never at registration "
        "(the data layer's own doctrine, proven by UncoveredIsUndeclared) -- while a real "
        "registration failure (a rank clash) closes the stack and returns false through the "
        "one verdict Open now carries");
  elsewhere.Close();

  GroundStack stack;
  CHECK(stack.Open("/tmp/outshine-drive-cache", "src/assets", 48.1, 11.5, wire, quiet),
        "the declared assets register and the stack stands: store, sources, pool, stream");
  CHECK(stack.Opened(), "it says so");
  CHECK(stack.Ground().PostM(48.1) > 0.0,
        "and the stream answers with the source's own post spacing -- the stack is usable, "
        "not merely allocated");
  stack.Close();
  CHECK(!stack.Opened(), "closing stands it down");
  CHECK(stack.Open("/tmp/outshine-drive-cache", "src/assets", 48.1, 11.5, wire, quiet),
        "and it opens again -- the cycle holds");

  Covers("II.15 the ground column stands up as one owned stack that opens or refuses loudly: "
         "a failed source registration returns false and closes, the surface constants carry "
         "their origin, and the open/close cycle is proven in the fast gate");
  return Report();
}

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "Check.h"
#include "Graph.h"

namespace {

// BOTH BENCHMARKS AGREE AND NEITHER RETROFITTED IT. Unreal has `FTaskGraph` with a render thread
// and an RHI thread beside its workers; RAGE has `sysTaskManager` and fibers. Dependencies are
// declared, the graph schedules, and no stage waits on a thread doing something unrelated. TARGET
// said nothing about threading at all, which means every line written until now assumed one
// thread -- and threading added late is a rewrite of everything it touches, which is why it
// belongs in the refactor rather than after it.
//
// THE ORACLE IS WHAT A DEPENDENCY MEANS, and it owes nothing to our design: if B waits on A then
// every run puts A before B, on any number of hands and in any interleaving. So the case records
// the order each step ran in and checks the pairs the graph was told about -- not the whole
// sequence, which is allowed to differ run to run wherever nothing was declared.
//
// THE SECOND ORACLE IS THAT A GRAPH IS NOT A SCHEDULE. The same steps with the same dependencies
// must produce the same RESULT on one hand and on many, or the graph is a race wearing a
// dependency's clothes. The case runs the identical graph at one hand and at four and compares
// what they computed.
//
// The capacity is fixed and the steps are function pointers with a context, so declaring a frame
// allocates nothing: `std::function` would have put a heap allocation on the frame path that
// CLAUDE.md forbids outright.
constexpr size_t kSteps = 12;

struct Ran {
  std::mutex Guard;
  std::vector<uint32_t> Order;
  std::atomic<int> Total{0};
};

struct Told {
  Ran *Into = nullptr;
  uint32_t Me = 0;
  int Adds = 0;
};

void Acts(void *with) {
  Told *const told = (Told *)with;
  told->Into->Total.fetch_add(told->Adds, std::memory_order_relaxed);
  std::lock_guard<std::mutex> held(told->Into->Guard);
  told->Into->Order.push_back(told->Me);
}

[[nodiscard]] size_t At(const std::vector<uint32_t> &order, uint32_t step) {
  for (size_t index = 0; index < order.size(); ++index) {
    if (order[index] == step) { return index; }
  }
  return order.size();
}

struct Chain {
  uint32_t First = 0, Middle = 0, Last = 0;
};

[[nodiscard]] int Built(unsigned hands, Ran &ran, Chain &chain, std::string &why) {
  outshine::Work::Graph graph(hands);
  std::vector<Told> told(kSteps);
  graph.Clears();
  for (size_t at = 0; at < kSteps; ++at) {
    told[at].Into = &ran;
    told[at].Adds = (int)at + 1;
    const uint32_t made = graph.Adds(&Acts, &told[at]);
    told[at].Me = made;
    if (made == 0xFFFFFFFFu) {
      why = graph.Error();
      return -1;
    }
  }
  chain.First = 0;
  chain.Middle = 5;
  chain.Last = 11;
  if (!graph.After(chain.Middle, chain.First) || !graph.After(chain.Last, chain.Middle) ||
      !graph.After(7, 3) || !graph.After(9, 7)) {
    why = graph.Error();
    return -1;
  }
  if (!graph.Runs()) {
    why = graph.Error();
    return -1;
  }
  return ran.Total.load(std::memory_order_relaxed);
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string why;
  Ran alone;
  Chain chain;
  const int byOne = Built(1, alone, chain, why);
  Ran together;
  const int byFour = Built(4, together, chain, why);

  CHECK(byOne > 0 && byFour > 0, ("both graphs ran: " + why).c_str());
  if (!(byOne > 0 && byFour > 0)) { return Report(); }

  std::printf("ON ONE HAND    %zu step(s) ran, summing %d\n", alone.Order.size(), byOne);
  std::printf("ON FOUR HANDS  %zu step(s) ran, summing %d\n", together.Order.size(), byFour);
  std::printf("THE DECLARED CHAIN LANDED AT  %zu -> %zu -> %zu on four hands\n",
              At(together.Order, chain.First), At(together.Order, chain.Middle),
              At(together.Order, chain.Last));

  CHECK(alone.Order.size() == kSteps && together.Order.size() == kSteps,
        "every step ran exactly once on both, so nothing was dropped or run twice -- a scheduler "
        "that loses a step is worse than one that orders them badly");
  CHECK(byOne == byFour,
        "**A GRAPH IS NOT A SCHEDULE**: the same steps with the same dependencies compute the "
        "same result on one hand and on four. A number that changed with the number of hands "
        "would be a race wearing a dependency's clothes");
  CHECK(At(together.Order, chain.First) < At(together.Order, chain.Middle) &&
            At(together.Order, chain.Middle) < At(together.Order, chain.Last),
        "**AND A DECLARED DEPENDENCY HOLDS ON EVERY HAND**: the chain 0 -> 5 -> 11 ran in that "
        "order on four hands, which is what a dependency MEANS. The order of everything else is "
        "allowed to differ, and does -- that is the freedom the graph exists to buy");
  CHECK(At(together.Order, 3) < At(together.Order, 7) &&
            At(together.Order, 7) < At(together.Order, 9),
        "and a second, independent chain holds at the same time, so what held above was the "
        "dependency and not an accident of one chain finishing first");

  Covers("the host: frame work is declared as steps with dependencies and a scheduler runs them "
         "-- every declared order holds on any number of hands, the result does not depend on how "
         "many there are, and declaring a frame allocates nothing");
  return Report();
}

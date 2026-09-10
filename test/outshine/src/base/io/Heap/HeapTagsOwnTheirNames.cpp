#include "Heap.h"
#include "Check.h"
#include <array>
#include <charconv>
#include <string_view>
#include <thread>

namespace {
void Allocate() {
  void *block = outshine::Heap::TryTake(32);
  outshine::Heap::Return(block);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const size_t baseline = Heap::LiveBytes();
  char name[] = "temporary-tag";
  {
    Heap::Tagged scope(name);
    Allocate();
  }
  name[0] = 'X';
  bool retained = false;
  for (size_t i = 0; i < Heap::TagCount(); ++i) {
    const char *tag = Heap::TagAt(i);
    retained |= tag != nullptr && std::string_view(tag) == "temporary-tag";
  }
  CHECK(retained, "tag name survives mutation of caller storage");
  const size_t before = Heap::TakenUnder("temporary-tag");
  char equal[] = "temporary-tag";
  {
    Heap::Tagged scope(equal);
    Allocate();
  }
  CHECK(Heap::TakenUnder("temporary-tag") > before, "equal text shares counters across addresses");
  const size_t outerBefore = Heap::TakenUnder("outer");
  const size_t innerBefore = Heap::TakenUnder("inner");
  {
    Heap::Tagged outer("outer");
    {
      Heap::Tagged inner("inner");
      Allocate();
    }
    Allocate();
  }
  CHECK(Heap::TakenUnder("outer") > outerBefore && Heap::TakenUnder("inner") > innerBefore,
        "nested scopes restore previous attribution");
  const size_t parallelBefore = Heap::TakenUnder("parallel");
  std::array<std::thread, 4> workers;
  for (auto &worker : workers) {
    worker = std::thread([] {
      char local[] = "parallel";
      Heap::Tagged scope(local);
      Allocate();
    });
  }
  for (auto &worker : workers) { worker.join(); }
  CHECK(Heap::TakenUnder("parallel") >= parallelBefore + 4 * 32,
        "threads share owned label without losing counts");
  const size_t overflowBefore = Heap::TakenUnder("other");
  std::array<char, 128> longName{};
  longName.fill('x');
  longName.back() = '\0';
  {
    Heap::Tagged scope(longName.data());
    Allocate();
  }
  CHECK(Heap::TakenUnder("other") > overflowBefore,
        "overlong name is accounted without truncation");
  for (size_t i = 0; i < Heap::TagCount(); ++i) {
    std::array<char, 32> tag{};
    const auto written = std::to_chars(tag.data(), tag.data() + tag.size() - 1, i);
    CHECK(written.ec == std::errc{}, "tag fixture fits");
    Heap::Tagged scope(tag.data());
    Allocate();
  }
  for (size_t i = 0; i < Heap::TagCount(); ++i) {
    CHECK(Heap::TagAt(i) != nullptr, "all fixed slots publish complete names");
  }
  const size_t fullBefore = Heap::TakenUnder("other");
  {
    Heap::Tagged scope("unregistered-after-capacity");
    Allocate();
  }
  CHECK(Heap::TakenUnder("other") > fullBefore, "full registry attributes new names to overflow");
  CHECK(Heap::LiveBytes() == baseline, "explicit allocations balanced");
  return Report();
}

#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Heap.h"

using outshine::Heap;

namespace {

size_t TakenEverywhere() {
  size_t sum = 0;
  for (size_t at = 0; at < Heap::TagCount(); ++at) { sum += Heap::TakenAt(at); }
  return sum;
}

size_t Named() {
  size_t named = 0;
  for (size_t at = 0; at < Heap::TagCount(); ++at) {
    if (Heap::TagAt(at) != nullptr) { ++named; }
  }
  return named;
}

const char kTags[64][12] = {
    "audit-00", "audit-01", "audit-02", "audit-03", "audit-04", "audit-05", "audit-06", "audit-07",
    "audit-08", "audit-09", "audit-10", "audit-11", "audit-12", "audit-13", "audit-14", "audit-15",
    "audit-16", "audit-17", "audit-18", "audit-19", "audit-20", "audit-21", "audit-22", "audit-23",
    "audit-24", "audit-25", "audit-26", "audit-27", "audit-28", "audit-29", "audit-30", "audit-31",
    "audit-32", "audit-33", "audit-34", "audit-35", "audit-36", "audit-37", "audit-38", "audit-39",
    "audit-40", "audit-41", "audit-42", "audit-43", "audit-44", "audit-45", "audit-46", "audit-47",
    "audit-48", "audit-49", "audit-50", "audit-51", "audit-52", "audit-53", "audit-54", "audit-55",
    "audit-56", "audit-57", "audit-58", "audit-59", "audit-60", "audit-61", "audit-62", "audit-63"};

constexpr size_t kBlock = 4096;

volatile size_t gSize = kBlock;
void *volatile gSink = nullptr;

void TakeAndReturn() {
  void *const block = ::operator new((size_t)gSize);
  gSink = block;
  ::operator delete(block);
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Note("tag slots the heap declares", (double)Heap::TagCount(), "slots");
  Note("slots already named before this test", (double)Named(), "slots");

  const size_t before = TakenEverywhere();
  {
    const Heap::Tagged one("audit-one");
    TakeAndReturn();
  }
  const size_t afterOne = TakenEverywhere();
  CHECK(afterOne >= before + kBlock,
        "a tagged allocation lands somewhere in the table, so the sum over every slot rises by at "
        "least what was taken");
  CHECK(Heap::TakenUnder("audit-one") >= kBlock,
        "and it lands under the tag that was open, which is what makes the table an attribution "
        "rather than a total");

  const size_t beforeMany = TakenEverywhere();
  size_t asked = 0;
  for (const auto &tag : kTags) {
    const Heap::Tagged one(tag);
    TakeAndReturn();
    ++asked;
  }
  const size_t afterMany = TakenEverywhere();
  Note("distinct tags asked for", (double)asked, "tags");
  Note("slots named after asking", (double)Named(), "slots");
  Note("bytes the table gained", (double)(afterMany - beforeMany), "bytes");

  CHECK(asked > Heap::TagCount(),
        "the test asks for more tags than the table has slots, or it is not exercising the bound");
  CHECK(Named() == Heap::TagCount(),
        "every slot is claimed once more tags than slots have been asked for");
  CHECK(afterMany - beforeMany >= asked * kBlock,
        "**EVERY BYTE IS STILL COUNTED**, so a tag beyond the table's reach costs an ATTRIBUTION "
        "and never a measurement -- an instrument that silently dropped the overflow would have "
        "made every number taken through it a lower bound nobody could see");

  CHECK(Heap::TakenUnder("other") > 0,
        "and what could not be named lands under 'other', which is a row a reader can see rather "
        "than a difference they would have to compute");

  bool overflowIsNamed = false;
  for (size_t at = 0; at < Heap::TagCount(); ++at) {
    const char *tag = Heap::TagAt(at);
    if (tag != nullptr && std::string(tag) == "other") { overflowIsNamed = true; }
  }
  CHECK(overflowIsNamed,
        "the overflow row is IN the table and reported like any other, because a row that existed "
        "only inside the allocator would be a number with no reader");

  Covers("I.9 the memory ledger attributes every byte it takes: to the tag that was open where the "
         "table can hold it, and to a named overflow row where it cannot, so a bound on the "
         "attribution is never a bound on the measurement");
  return Report();
}

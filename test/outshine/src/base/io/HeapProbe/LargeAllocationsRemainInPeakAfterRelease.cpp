#include "HeapProbe.h"
#include "Check.h"

#include <cstddef>
#include <cstdlib>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t kBytes = size_t{64} * 1024u * 1024u;

  HeapProbe::ForgetPeak();
  const size_t before = HeapProbe::Sample();
  auto *block = static_cast<unsigned char *>(std::malloc(kBytes));
  CHECK(block != nullptr, "the large allocation succeeds");
  if (block == nullptr) { return Report(); }
  static_cast<volatile unsigned char *>(block)[0] = 1;
  static_cast<volatile unsigned char *>(block)[kBytes - 1] = 1;
  const size_t during = HeapProbe::Sample();
  const size_t reserved = HeapProbe::BreakBytes();
  std::free(block);
  const size_t after = HeapProbe::Sample();
  const size_t peak = HeapProbe::PeakLiveBytes();

  CHECK(during >= before && during - before >= kBytes,
        "live allocator bytes include the large allocation");
  CHECK(during >= after && during - after >= kBytes,
        "freeing the large allocation removes its live bytes");
  CHECK(reserved >= during, "reserved allocator bytes cover the live allocation");
  CHECK(peak == during, "freeing the allocation preserves the observed peak");

  HeapProbe::ForgetPeak();
  CHECK(HeapProbe::PeakLiveBytes() == 0, "an explicit reset clears the observed peak");
  const size_t resampled = HeapProbe::Sample();
  CHECK(HeapProbe::PeakLiveBytes() == resampled,
        "a reset starts a new peak at the current live allocation");
  return Report();
}

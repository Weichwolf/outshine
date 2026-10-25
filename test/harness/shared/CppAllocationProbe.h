#ifndef OUTSHINE_TEST_CPPALLOCATIONPROBE_H
#define OUTSHINE_TEST_CPPALLOCATIONPROBE_H

#include <cstddef>

namespace outshine::Test {
struct CppAllocationCounts {
  size_t Calls = 0;
  size_t Bytes = 0;
};

void BeginCppAllocationProbe() noexcept;
[[nodiscard]] CppAllocationCounts EndCppAllocationProbe() noexcept;
}
#endif

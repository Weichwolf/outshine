/* THE HARNESS'S CLOCK, and it is compiled because there is nothing else to ask. macOS has no
 * `timeout(1)` and its `date(1)` has no sub-second field, and run.sh declares its whole dependency
 * set as the shell, the compiler and nm — so the millisecond comes from the compiler. */
#include <chrono>
#include <cstdio>

int main() {
  const auto since = std::chrono::system_clock::now().time_since_epoch();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(since).count();
  std::printf("%lld\n", (long long)ms);
  return 0;
}

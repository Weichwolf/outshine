#include <chrono>
#include <cstdio>

int main() {
  const auto since = std::chrono::system_clock::now().time_since_epoch();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(since).count();
  std::printf("%lld\n", (long long)ms);
  return 0;
}

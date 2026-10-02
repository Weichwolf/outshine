#include "Fetching.h"
#include "Check.h"

#include <chrono>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Fetching transport({});
  const auto began = std::chrono::steady_clock::now();
  const bool progressed = transport.Await(50.0);
  const auto seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  CHECK(!progressed, "an idle transport reports no network completion");
  CHECK(seconds >= 0.04 && seconds < 1.0,
        "an idle retry wait parks for its bounded timer instead of returning immediately");
  return Report();
}

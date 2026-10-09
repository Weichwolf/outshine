#include "GroundRegionPreparation.h"
#include "Check.h"
#include <chrono>
#include <semaphore>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace std::chrono_literals;
  Tasks pool(1);
  std::binary_semaphore entered(0);
  std::binary_semaphore release(0);
  GroundRegionPreparation work(
      pool, [&](std::stop_token) -> std::expected<GroundRegionPreparation::Completed, std::string> {
        entered.release();
        release.acquire();
        return GroundRegionPreparation::Completed{
            .Key = "native-product", .RequestKey = "planned-demand", .RequestHit = true};
      });
  const bool running = entered.try_acquire_for(5s);
  CHECK(running, "region lookup starts on the compute worker");
  CHECK(!work.Peek() && !work.Collect(), "running lookup exposes no partially written product");
  release.release();
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!work.Peek() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(1ms);
  }
  const auto viewed = work.Peek();
  CHECK(viewed && *viewed && viewed->value().Key == "native-product" &&
            viewed->value().RequestKey == "planned-demand" && viewed->value().RequestHit,
        "completed immutable metadata can be inspected before ownership transfer");
  const auto transferred = work.Collect();
  CHECK(transferred && *transferred && transferred->value().Key == "native-product",
        "inspection leaves the result available to the ground candidate");
  CHECK(!work.Peek() && !work.Collect(), "completed result transfers exactly once");
  return Report();
}

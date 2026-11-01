#include "AssetPipeline.h"
#include "Check.h"
#include <any>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <semaphore>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr std::string_view kAddress =
    "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
constexpr size_t kBytes = 1024;

struct Metadata {
  int Code = 0;
  std::string Identity;
};

AssetPublication Await(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

AssetFailure Failure(size_t extra = 0) {
  Metadata data{.Code = 71, .Identity = std::string(128, 'r')};
  const auto bytes = sizeof(data) + data.Identity.capacity() + extra;
  return {.Message = "input unavailable", .Detail = std::move(data), .Bytes = bytes};
}

void Shared() {
  std::shared_ptr<const std::any> retained;
  std::weak_ptr<const std::any> lifetime;
  {
    Tasks pool(1);
    AssetPipeline pipeline(pool,
                           {.JobsMost = 1,
                            .ConsumersMost = 2,
                            .TicketsMost = 2,
                            .InputBytesMost = kBytes,
                            .ReceiptBytesMost = kBytes});
    std::atomic_uint calls{0};
    std::binary_semaphore entered(0);
    std::binary_semaphore release(0);
    {
      auto first = pipeline.OpenConsumer();
      auto second = pipeline.OpenConsumer();
      CHECK(first && second, "two consumers fit the shared error-metadata budget");
      if (!first || !second) { return; }
      const auto produce =
          [&](const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, AssetFailure> {
        ++calls;
        entered.release();
        (void)release.try_acquire_for(std::chrono::seconds(5));
        return std::unexpected(Failure());
      };
      auto a = pipeline.RequestLogical(
          *first,
          {.Address = kAddress, .Generation = first->Generation(), .ReceiptBytesMost = kBytes},
          produce);
      CHECK(a && entered.try_acquire_for(std::chrono::seconds(5)), "the shared producer enters");
      auto b = pipeline.RequestLogical(
          *second,
          {.Address = kAddress, .Generation = second->Generation(), .ReceiptBytesMost = kBytes},
          produce);
      release.release();
      CHECK(a && b, "both tickets share the same in-flight logical request");
      if (!a || !b) { return; }
      const auto left = Await(*a);
      const auto right = Await(*b);
      CHECK(left.Status == AssetPublicationStatus::Failed && right.Status == left.Status &&
                left.Error == AssetPublicationError::ProducerFailed && !left.Receipt &&
                !left.Native && left.Failure && left.Failure == right.Failure && calls == 1,
            "one terminal failure carries the same typed context to both consumers");
      if (!left.Failure) { return; }
      const auto *detail = std::any_cast<Metadata>(left.Failure.get());
      CHECK(detail && detail->Code == 71 && detail->Identity == std::string(128, 'r'),
            "failure details preserve their full typed values");
      retained = left.Failure;
      lifetime = retained;
      a->Cancel();
      const auto cancelled = a->Poll();
      CHECK(cancelled.Status == AssetPublicationStatus::Cancelled && !cancelled.Failure,
            "cancelled demand does not expose completed failure context");
      CHECK(second->Advance({.Value = 2}), "the other consumer advances its generation");
      const auto stale = b->Poll();
      CHECK(stale.Status == AssetPublicationStatus::Cancelled &&
                stale.Error == AssetPublicationError::StaleGeneration && !stale.Failure,
            "a newer world generation cannot receive stale failure details");
    }
    const auto held = pipeline.Counts();
    CHECK(held.Jobs == 0 && held.Consumers == 0 && held.Tickets == 0 && held.InputBytes == 0 &&
              held.ReceiptBytes == kBytes,
          "the last failure alias retains metadata admission after all tickets retire");
  }
  CHECK(!lifetime.expired() && std::any_cast<Metadata>(retained.get()) != nullptr,
        "typed error metadata survives dispatcher facade retirement");
  retained.reset();
  CHECK(lifetime.expired(), "the last alias releases the stored failure and its reservation");
}

void Oversize() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = kBytes,
                          .ReceiptBytesMost = kBytes});
  auto consumer = pipeline.OpenConsumer();
  CHECK(consumer, "the bounded failure consumer opens");
  if (!consumer) { return; }
  auto ticket = pipeline.RequestLogical(
      *consumer,
      {.Address = kAddress, .Generation = consumer->Generation(), .ReceiptBytesMost = kBytes},
      [](const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, AssetFailure> {
        return std::unexpected(Failure(kBytes));
      });
  CHECK(ticket, "the producer owns its declared receipt reservation");
  if (!ticket) { return; }
  const auto result = Await(*ticket);
  CHECK(result.Status == AssetPublicationStatus::Failed &&
            result.Error == AssetPublicationError::InvalidReceipt && !result.Failure &&
            !result.Receipt,
        "oversized failure context cannot escape the admitted metadata budget");
}
}

int main() {
  Shared();
  Oversize();
  return outshine::Test::Report();
}

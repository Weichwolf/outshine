#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
const std::string kContent(64, 'a');

AssetRecord Record() {
  return {.Key = kContent,
          .Kind = "bounded-logical-alias-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'd'),
          .ByteCount = 3,
          .Parent = {}};
}

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

bool Drain(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  if (barrier == Tasks::kNoTask || !pool.AwaitCompletion(barrier, 5)) { return false; }
  (void)pool.TakeCompletion(barrier);
  return true;
}

std::string Address(size_t index) {
  constexpr const char *digits = "0123456789abcdef";
  std::string result(64, 'c');
  for (size_t offset = 0; offset < 16; ++offset) {
    result[63 - offset] = digits[index & 15U];
    index >>= 4U;
  }
  return result;
}

void BoundedAliases() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 3,
                          .InputBytesMost = 256,
                          .ReceiptBytesMost = 2048});
  std::atomic_size_t discoveries{0};
  std::atomic_size_t duplicates{0};
  {
    auto owner = pipeline.OpenConsumer();
    auto queries = pipeline.OpenConsumer();
    CHECK(owner && queries, "the content owner and logical caller fit their two consumer slots");
    if (!owner || !queries) { return; }
    auto content = pipeline.RequestResolved(
        *owner,
        {.Key = AssetProductKey::Parse(kContent).value_or(AssetProductKey{}),
         .Generation = owner->Generation(),
         .ConsumerBytes = 8,
         .ReceiptBytesMost = 1024},
        [](const std::stop_token &) -> std::expected<AssetProduct, std::string> {
          return AssetProduct(Record());
        });
    CHECK(content.has_value(), "the actual content has a long-lived direct demand");
    if (!content) { return; }
    const auto direct = Wait(*content);
    CHECK(direct.Status == AssetPublicationStatus::Ready && direct.Receipt &&
              !direct.Receipt->RequestKey && !direct.RequestKey,
          "the retained content receipt is immutable and independent of logical addresses");
    constexpr size_t kRequests = 64;
    for (size_t index = 0; index < kRequests; ++index) {
      const auto address = Address(index);
      const AssetPipeline::LogicalRequest request{.Address = address,
                                                  .Generation = queries->Generation(),
                                                  .InputBytes = 32,
                                                  .ConsumerBytes = 8,
                                                  .ReceiptBytesMost = 1024};
      auto first = pipeline.RequestLogical(
          *queries,
          request,
          [&discoveries, &duplicates](
              const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, std::string> {
            ++discoveries;
            return AssetPipeline::ResolvedWork{
                .Key = AssetProductKey::Parse(kContent).value_or(AssetProductKey{}),
                .Produce = [&duplicates](const std::stop_token &)
                    -> std::expected<std::optional<AssetProduct>, std::string> {
                  ++duplicates;
                  return AssetProduct(Record());
                }};
          });
      auto second = pipeline.RequestLogical(
          *queries,
          request,
          [&duplicates](
              const std::stop_token &) -> std::expected<std::optional<AssetProduct>, std::string> {
            ++duplicates;
            return std::unexpected("an existing logical address repeated its discovery");
          });
      CHECK(first && second, "two demands for the current address share bounded admission");
      if (!first || !second) { break; }
      const auto left = Wait(*first);
      const auto right = Wait(*second);
      CHECK(left.Status == AssetPublicationStatus::Ready &&
                right.Status == AssetPublicationStatus::Ready && left.Receipt == direct.Receipt &&
                right.Receipt == direct.Receipt &&
                left.RequestKey == AssetRequestKey::Parse(address) &&
                left.RequestKey == right.RequestKey,
            "both logical demands retain their address and share the original content receipt");
      first = std::unexpected(AssetPublicationError::InvalidRequest);
      CHECK(pipeline.Counts().Addresses == 2,
            "the logical directory entry survives its first consumer release");
      second = std::unexpected(AssetPublicationError::InvalidRequest);
      CHECK(Drain(pool), "retired discovery inputs remain charged through actual worker cleanup");
      const auto state = pipeline.Counts();
      CHECK(state.Jobs == 1 && state.Addresses == 1 && state.Tickets == 1 &&
                state.InputBytes == 8 && state.ReceiptBytes == 1024,
            "the last logical ticket retires its address while the shared content stays resident");
      if (state.Addresses != 1) { break; }
    }
    CHECK(discoveries.load() == kRequests && duplicates.load() == 0,
          "changing addresses performs one discovery each and never repeats content production");
  }
  const auto final = pipeline.Counts();
  CHECK(final.Jobs == 0 && final.Addresses == 0 && final.Consumers == 0 && final.Tickets == 0 &&
            final.InputBytes == 0 && final.ReceiptBytes == 0,
        "the final content demand releases every directory entry and reservation");
}
}

int main() {
  BoundedAliases();
  return Report();
}

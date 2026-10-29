#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <optional>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr std::string_view kAddress =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

AssetRecord Record() {
  return {.Key = std::string(64, 'a'),
          .Kind = "logical-receipt-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'd'),
          .ByteCount = 3,
          .Parent = {},
          .RequestKey = std::string(kAddress)};
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

void Receipt(bool logical, AssetRecord record, bool valid) {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = 16,
                          .ReceiptBytesMost = 1024});
  auto consumer = pipeline.OpenConsumer();
  CHECK(consumer.has_value(), "the isolated receipt consumer opens");
  if (!consumer) { return; }
  const auto key = record.Key;
  const auto produce = [record = std::make_shared<AssetRecord>(std::move(record))](
                           const std::stop_token &) { return std::move(*record); };
  auto ticket =
      logical
          ? pipeline.RequestLogical(*consumer,
                                    {.Address = kAddress,
                                     .Generation = consumer->Generation(),
                                     .ReceiptBytesMost = 1024},
                                    [produce](const std::stop_token &stop)
                                        -> std::expected<std::optional<AssetRecord>, std::string> {
                                      return produce(stop);
                                    })
          : pipeline.RequestResolved(
                *consumer,
                {.Key = AssetProductKey::Parse(kAddress).value_or(AssetProductKey{}),
                 .Generation = consumer->Generation(),
                 .ReceiptBytesMost = 1024},
                produce);
  CHECK(ticket.has_value(), "receipt validation follows ordinary bounded job admission");
  if (!ticket) { return; }
  const auto result = Wait(*ticket);
  if (valid) {
    CHECK(result.Status == AssetPublicationStatus::Ready && result.Receipt &&
              result.Receipt->Key == key,
          "a valid publication retains the actual content key");
  } else {
    CHECK(result.Status == AssetPublicationStatus::Failed && !result.Receipt &&
              result.Error == AssetPublicationError::InvalidReceipt,
          "invalid or cross-domain receipts publish no product");
  }
}

void Contracts() {
  Receipt(true, Record(), true);
  auto record = Record();
  record.RequestKey.reset();
  Receipt(true, record, false);
  record = Record();
  record.RequestKey = std::string(64, 'c');
  Receipt(true, record, false);
  record = Record();
  record.RequestKey = "invalid-request";
  Receipt(true, record, false);
  record = Record();
  record.Key = "invalid-content";
  Receipt(true, record, false);
  record = Record();
  record.Package = "invalid-package";
  Receipt(true, record, false);
  record = Record();
  record.ByteCount = 0;
  Receipt(true, record, false);
  record = Record();
  record.Kind.clear();
  Receipt(true, record, false);
  record = Record();
  record.RequestKey.emplace(kAddress).reserve(4096);
  Receipt(true, std::move(record), false);
  record = Record();
  record.Key = kAddress;
  Receipt(true, record, true);
  Receipt(false, record, false);
  record.RequestKey.reset();
  Receipt(false, record, true);
  record.Key = std::string(64, 'a');
  Receipt(false, record, false);
}

void Admission() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = 16,
                          .ReceiptBytesMost = 1024});
  auto consumer = pipeline.OpenConsumer();
  if (!consumer) {
    CHECK(false, "admission consumer opens");
    return;
  }
  std::atomic<size_t> calls{0};
  const auto producer =
      [&calls](const std::stop_token &) -> std::expected<std::optional<AssetRecord>, std::string> {
    ++calls;
    return Record();
  };
  const auto invalid = pipeline.RequestLogical(
      *consumer,
      {.Address = "bad", .Generation = consumer->Generation(), .ReceiptBytesMost = 1024},
      producer);
  CHECK(!invalid && invalid.error() == AssetPublicationError::InvalidRequest,
        "an invalid address is rejected before job admission");
  const auto unbounded = pipeline.RequestLogical(
      *consumer, {.Address = kAddress, .Generation = consumer->Generation()}, producer);
  CHECK(!unbounded && unbounded.error() == AssetPublicationError::InvalidRequest,
        "logical requests require a receipt reservation too");
  const auto empty = pipeline.RequestLogical(
      *consumer,
      {.Address = kAddress, .Generation = consumer->Generation(), .ReceiptBytesMost = 1024},
      {});
  CHECK(!empty && empty.error() == AssetPublicationError::InvalidRequest && calls.load() == 0 &&
            pipeline.Counts().Jobs == 0 && pipeline.Counts().Tickets == 0,
        "invalid submissions retain no admission and execute no producer");
}
}

int main() {
  Contracts();
  Admission();
  return Report();
}

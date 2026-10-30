#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kRam = 128, kBlocked = 64, kLookup = 8, kRead = 96, kRestore = 16;
const std::string kAddress(64, 'b');

AssetRecord Record(char key, bool bound = false) {
  return {.Key = std::string(64, key),
          .Kind = "stage-admission-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'f'),
          .ByteCount = 1,
          .Parent = {},
          .RequestKey = bound ? std::optional(kAddress) : std::nullopt};
}

struct Progress {
  std::atomic<unsigned> Calls{0};
  std::atomic<size_t> ReadAdmission{0}, RestoreAdmission{0};
  std::optional<std::vector<uint8_t>> Buffer;
};

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

bool Waiting(const AssetPipeline &pipeline,
             const AssetPipeline::RequestTicket &ticket,
             const Progress &progress) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < until) {
    (void)ticket.Poll();
    if (progress.Calls.load() == 1 && pipeline.Counts().Running == 0) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

void GrowthAndShrink() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 2,
                          .InputBytesMost = kRam,
                          .ReceiptBytesMost = 4096});
  auto a = pipeline.OpenConsumer();
  auto b = pipeline.OpenConsumer();
  CHECK(a && b, "two independent consumers share the fixed RAM admission");
  if (!a || !b) { return; }
  std::atomic_bool release{false};
  const auto key = AssetProductKey::Parse(std::string(64, 'a'));
  CHECK(key.has_value(), "the competing content address is valid");
  if (!key) { return; }
  auto blocker = pipeline.RequestResumable(
      *a,
      {.Key = *key,
       .Generation = a->Generation(),
       .InputBytes = kBlocked,
       .ReceiptBytesMost = 2048},
      [&release](
          const std::stop_token &) -> std::expected<std::optional<AssetRecord>, std::string> {
        if (!release.load()) { return std::nullopt; }
        return Record('a');
      });
  const auto progress = std::make_shared<Progress>();
  auto ticket = pipeline.RequestLogical(
      *b,
      {.Address = kAddress,
       .Generation = b->Generation(),
       .InputBytes = kLookup,
       .ReceiptBytesMost = 2048},
      [&pipeline, progress](
          const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, std::string> {
        switch (progress->Calls.fetch_add(1)) {
          case 0: return AssetPipeline::InputAdmission{.Bytes = kRead};
          case 1:
            progress->ReadAdmission = pipeline.Counts().InputBytes;
            progress->Buffer.emplace(kRead, uint8_t{7});
            progress->Buffer.reset();
            return AssetPipeline::InputAdmission{.Bytes = kRestore};
          default:
            progress->RestoreAdmission = pipeline.Counts().InputBytes;
            return std::optional(AssetProduct(Record('c', true)));
        }
      });
  CHECK(blocker && ticket, "small lookup and retained work fit without reserving the read early");
  if (!blocker || !ticket) { return; }
  CHECK(Waiting(pipeline, *ticket, *progress) && progress->Calls == 1 && !progress->Buffer,
        "the expensive callback is not invoked or allocated while its next stage cannot fit");
  CHECK(pipeline.Counts().Jobs == 2 && pipeline.Counts().InputBytes == kBlocked + kLookup,
        "waiting keeps the same two physical jobs and only their actual admitted bounds");
  std::atomic_bool ran{false};
  const auto other = pool.Post([&ran] { ran = true; });
  CHECK(other != Tasks::kNoTask, "unrelated work can enter the same single-worker pool");
  if (other != Tasks::kNoTask) { pool.Wait(other); }
  CHECK(ran, "RAM backpressure consumes no worker while awaiting capacity");
  release = true;
  auto completed = Wait(*blocker);
  auto loaded = Wait(*ticket);
  CHECK(completed.Status == AssetPublicationStatus::Ready &&
            loaded.Status == AssetPublicationStatus::Ready && loaded.Receipt &&
            loaded.RequestKey == AssetRequestKey::Parse(kAddress),
        "freeing the competing input lets the same logical ticket finish with its own address");
  CHECK(progress->Calls == 3 && progress->ReadAdmission == kRead &&
            progress->RestoreAdmission == kRestore && !progress->Buffer,
        "growth is admitted before allocation and shrinking follows actual buffer release");
  blocker = std::unexpected(AssetPublicationError::InvalidRequest);
  ticket = std::unexpected(AssetPublicationError::InvalidRequest);
  completed = {};
  loaded = {};
  CHECK(pipeline.Counts().Jobs == 0 && pipeline.Counts().InputBytes == 0 &&
            pipeline.Counts().ReceiptBytes == 0,
        "all stage reservations and physical jobs retire after their final handles");
}

void ImpossibleStage() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = kRam,
                          .ReceiptBytesMost = 2048});
  auto consumer = pipeline.OpenConsumer();
  CHECK(consumer.has_value(), "an impossible-stage consumer opens");
  if (!consumer) { return; }
  std::atomic_uint calls{0};
  auto ticket = pipeline.RequestLogical(
      *consumer,
      {.Address = kAddress,
       .Generation = consumer->Generation(),
       .InputBytes = kLookup,
       .ReceiptBytesMost = 2048},
      [&calls](const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, std::string> {
        ++calls;
        return AssetPipeline::InputAdmission{.Bytes = kRam + 1};
      });
  CHECK(ticket.has_value(), "lookup is admitted before its next cost is discovered");
  if (!ticket) { return; }
  const auto result = Wait(*ticket);
  CHECK(result.Status == AssetPublicationStatus::Failed &&
            result.Error == AssetPublicationError::InvalidRequest && calls == 1 &&
            pipeline.Counts().InputBytes == 0,
        "a stage larger than the unchanged total fails before a second callback or allocation");
}

void CancelWaitingStage(bool advance) {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 3,
                          .InputBytesMost = kRam,
                          .ReceiptBytesMost = 4096});
  auto owner = pipeline.OpenConsumer();
  auto observer = pipeline.OpenConsumer();
  CHECK(owner && observer, "waiting-stage cancellation opens independent consumers");
  if (!owner || !observer) { return; }
  std::atomic_bool release{false};
  const auto key = AssetProductKey::Parse(std::string(64, 'a'));
  CHECK(key.has_value(), "the independent content address is valid");
  if (!key) { return; }
  auto blocker = pipeline.RequestResumable(
      *observer,
      {.Key = *key,
       .Generation = observer->Generation(),
       .InputBytes = kBlocked,
       .ReceiptBytesMost = 2048},
      [&release](
          const std::stop_token &) -> std::expected<std::optional<AssetRecord>, std::string> {
        if (!release.load()) { return std::nullopt; }
        return Record('a');
      });
  const auto progress = std::make_shared<Progress>();
  auto waiting = pipeline.RequestLogical(
      *owner,
      {.Address = kAddress,
       .Generation = owner->Generation(),
       .InputBytes = kLookup,
       .ReceiptBytesMost = 2048},
      [progress](
          const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, std::string> {
        ++progress->Calls;
        return AssetPipeline::InputAdmission{.Bytes = kRead};
      });
  CHECK(blocker && waiting, "competing work and a lookup enter the shared admission");
  if (!blocker || !waiting) { return; }
  CHECK(Waiting(pipeline, *waiting, *progress), "the requested growth waits before allocation");
  if (advance) {
    CHECK(owner->Advance(AssetConsumerGeneration{2}), "a new generation cancels waiting growth");
  } else {
    waiting->Cancel();
  }
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (pipeline.Counts().InputBytes != kBlocked && std::chrono::steady_clock::now() < until) {
    (void)waiting->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto cancelled = waiting->Poll();
  CHECK(cancelled.Status == AssetPublicationStatus::Cancelled && !cancelled.Receipt &&
            !cancelled.Native && progress->Calls == 1,
        "cancellation never invokes the unadmitted callback or exposes a product");
  CHECK(pipeline.Counts().InputBytes == kBlocked,
        "retired waiting work releases its old bound without touching another job");
  waiting = std::unexpected(AssetPublicationError::InvalidRequest);
  release = true;
  auto completed = Wait(*blocker);
  CHECK(completed.Status == AssetPublicationStatus::Ready,
        "cancelling waiting growth preserves independent work");
  blocker = std::unexpected(AssetPublicationError::InvalidRequest);
  completed = {};
  CHECK(pipeline.Counts().Jobs == 0 && pipeline.Counts().InputBytes == 0 &&
            pipeline.Counts().ReceiptBytes == 0,
        "cancellation and generation retirement leave no admission behind");
}
}

int main() {
  GrowthAndShrink();
  ImpossibleStage();
  CancelWaitingStage(false);
  CancelWaitingStage(true);
  return Report();
}

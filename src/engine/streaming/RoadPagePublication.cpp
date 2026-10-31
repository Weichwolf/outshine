#include "RoadPageQueueWork.h"
#include "RoadPageProducer.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
namespace {
constexpr size_t kReceiptBytesMost = 4096;

size_t RequestBytes(const Generators::RoadPageRequest &request) {
  return sizeof(request) + request.Address.PlanKey.capacity() +
         request.Address.MaterialKey.capacity() +
         Generators::RoadPageAppearanceCapacityBytes(request.Address.Appearance) +
         request.Address.Parts.capacity() * sizeof(Generators::RoadPagePart) +
         request.Parent.capacity() + request.RequestKey.capacity();
}

struct PagePublication {
  std::shared_ptr<Generators::PreparedRoadPages> Pages;
  std::optional<Generators::RoadPageRequest> Request;
  std::shared_ptr<const Generators::RoadPageProducer> Producer;
  std::optional<Generators::PreparedRoadPages::PageWrite> Write;
  size_t PackageBytesMost = 0;

  std::expected<std::optional<AssetProduct>, std::string> Step(const std::stop_token &stop) {
    using Cache = Generators::PreparedRoadPages;
    if (!Write) {
      if (!Request) { return std::unexpected("road publication lost its request"); }
      auto prepared = Cache::PreparePage(std::move(*Request));
      Request.reset();
      if (!prepared) { return std::unexpected(Cache::DescribeFailure(prepared.error())); }
      Write.emplace(std::move(*prepared));
    }
    auto record = Pages->Prepare(*Write, Producer.get(), stop);
    if (!Write->NeedsProducer()) { Producer.reset(); }
    if (Write->PublicationBytes() > PackageBytesMost) {
      return std::unexpected("road page publication exceeds its package capacity");
    }
    if (!record) {
      const auto *cache = std::get_if<AssetCacheError>(&record.error());
      if (cache != nullptr && *cache == AssetCacheError::Busy) { return std::nullopt; }
      return std::unexpected(Cache::DescribeFailure(record.error()));
    }
    return AssetProduct(std::move(*record));
  }
};

struct BoundReceipt {
  AssetRecord Record;
  RoadPageQueue::Receipt Publication;
  AssetPipeline::RequestTicket Retention;
};
}

std::expected<bool, std::string>
RoadPageQueue::RequestRecord(Generators::RoadPageRequest request,
                             std::shared_ptr<const Generators::RoadPageProducer> producer) {
  if (!Consumer_ && Publications_) {
    auto consumer = Publications_->OpenConsumer({.Identity = Pages_});
    if (!consumer) {
      if (consumer.error() == AssetPublicationError::ResourceDeferred) { return false; }
      return std::unexpected("road page consumer admission failed");
    }
    Consumer_.emplace(std::move(*consumer));
  }
  if (!Consumer_ || request.NativeBytesMost == 0 ||
      request.NativeBytesMost != Limits_.PageBytesMost ||
      (!request.Parent.empty() && !AssetProductKey::Parse(request.Parent)) ||
      (!request.RequestKey.empty() && !AssetProductKey::Parse(request.RequestKey))) {
    return std::unexpected("invalid road publication request or unavailable asset pipeline");
  }
  const auto key = Generators::RoadPageKey(request.Address);
  const auto product = AssetProductKey::Parse(key);
  if (!product) { return std::unexpected("invalid road page product address"); }
  const auto admission = RequestState(key, Kind::Publication);
  if (admission == Admission::Pending) {
    const auto &work = Work_.find(key)->second;
    if (work->Parent != request.Parent || work->RequestKey != request.RequestKey) {
      return std::unexpected("road page request conflicts with its pending binding");
    }
    return true;
  }
  if (admission != Admission::Available) { return false; }
  auto context = request;
  request.RequestKey.clear();
  const auto requestBytes = RequestBytes(request);
  const auto producerBytes = producer ? producer->InputCapacityBytes() : 0;
  if (producerBytes > std::numeric_limits<size_t>::max() - requestBytes) {
    return std::unexpected("road page producer input budget overflows");
  }
  const auto inputBytes = requestBytes + producerBytes;
  if (request.NativeBytesMost > (std::numeric_limits<size_t>::max() - 2 * kReceiptBytesMost) / 2) {
    return std::unexpected("road page publication budget overflows");
  }
  const size_t packageBytesMost = 2 * request.NativeBytesMost + kReceiptBytesMost;
  auto publication =
      std::make_shared<PagePublication>(PagePublication{.Pages = Pages_,
                                                        .Request = std::move(request),
                                                        .Producer = std::move(producer),
                                                        .Write = {},
                                                        .PackageBytesMost = packageBytesMost});
  auto factory = [publication = std::move(publication)](const std::stop_token &stop) {
    return publication->Step(stop);
  };
  constexpr size_t kFactoryBytes = sizeof(factory) + sizeof(PagePublication);
  if (inputBytes > std::numeric_limits<size_t>::max() - kFactoryBytes) {
    return std::unexpected("road page publication input budget overflows");
  }
  auto ticket = Publications_->RequestResumable(
      *Consumer_,
      {.Key = *product,
       .Generation = Consumer_->Generation(),
       .InputBytes = inputBytes + kFactoryBytes,
       .ConsumerBytes = sizeof(Work) + RequestBytes(context) + kReceiptBytesMost,
       .ReceiptBytesMost = kReceiptBytesMost},
      std::move(factory));
  if (!ticket) {
    if (ticket.error() == AssetPublicationError::ResourceDeferred) { return false; }
    return std::unexpected("road page publication admission failed");
  }
  auto work = std::make_shared<Work>();
  work->Type = Kind::Publication;
  work->Parent = context.Parent;
  work->RequestKey = context.RequestKey;
  work->Request.emplace(std::move(context));
  work->Ticket.emplace(std::move(*ticket));
  Work_.emplace(key, std::move(work));
  return true;
}

void RoadPageQueue::PollPublication(const std::shared_ptr<Work> &work) {
  if (work->BindingQueued) {
    if (work->BindingState.load(std::memory_order_acquire) == Work::BindingStatus::Deferred &&
        std::chrono::steady_clock::now() >= work->RetryAt) {
      PostBinding(work);
    }
    return;
  }
  if (!work->Ticket || !work->Request) {
    work->Result = std::unexpected("road publication lost its admitted ticket or request");
    work->Complete.store(true, std::memory_order_release);
    return;
  }
  auto publication = work->Ticket->Poll();
  if (publication.Status == AssetPublicationStatus::Pending || !publication.Finished) { return; }
  if (publication.Status == AssetPublicationStatus::Cancelled || work->Stop.stop_requested()) {
    work->Result = std::unexpected("road page request canceled");
  } else if (publication.Status == AssetPublicationStatus::Failed) {
    work->Result = std::unexpected(std::move(publication.Message));
  } else if (work->RequestKey.empty()) {
    work->Result = std::optional<Product>(std::move(publication.Receipt));
  } else {
    work->Published = std::move(publication.Receipt);
    work->BindingQueued = true;
    const bool posted = Pool_.PostDetached([work, pages = Pages_] { RunBinding(work, pages); });
    if (posted) { return; }
    work->Result = std::unexpected("road page alias dispatcher stopped");
  }
  work->Complete.store(true, std::memory_order_release);
}

void RoadPageQueue::CompleteBinding(const std::shared_ptr<Work> &work) {
  if (!work->Ticket || !work->Bound) {
    work->Result = std::unexpected("road alias lost its admitted ticket or result");
    return;
  }
  const auto publication = work->Ticket->Poll();
  if (publication.Status == AssetPublicationStatus::Cancelled || work->Stop.stop_requested()) {
    work->Result = std::unexpected("road page request canceled");
  } else if (!*work->Bound) {
    work->Result = std::unexpected(std::move(work->Bound->error()));
  } else {
    const auto stored =
        std::make_shared<BoundReceipt>(BoundReceipt{.Record = std::move(work->Bound->value()),
                                                    .Publication = std::move(work->Published),
                                                    .Retention = std::move(*work->Ticket)});
    Receipt receipt(stored, &stored->Record);
    work->Result = std::optional<Product>(std::move(receipt));
  }
  work->Bound.reset();
}
}

#include "RoadPageQueueWork.h"
#include "RoadPageProducer.h"
#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
namespace {
constexpr size_t kReceiptBytesMost = 4096;

size_t RequestBytes(const Generators::RoadPageRequest &request) {
  return sizeof(request) + request.Address.PlanKey.capacity() +
         request.Address.MaterialKey.capacity() +
         request.Address.Parts.capacity() * sizeof(Generators::RoadPagePart) +
         request.Parent.capacity() + request.RequestKey.capacity();
}

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
    auto consumer = Publications_->OpenConsumer();
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
  const auto inputBytes = RequestBytes(request) + (producer ? producer->InputCapacityBytes() : 0);
  auto factory = [pages = Pages_, request = std::move(request), producer = std::move(producer)](
                     std::stop_token stop) {
    return pages->Prepare(request, producer.get(), stop);
  };
  auto ticket = Publications_->RequestResolved(
      *Consumer_,
      {.Key = *product,
       .Generation = Consumer_->Generation(),
       .InputBytes = inputBytes + sizeof(factory),
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
  if (work->BindingQueued) { return; }
  auto publication = work->Ticket->Poll();
  if (publication.Status == AssetPublicationStatus::Pending) { return; }
  if (publication.Status == AssetPublicationStatus::Cancelled || work->Stop.stop_requested()) {
    work->Result = std::unexpected("road page request canceled");
  } else if (publication.Status == AssetPublicationStatus::Failed) {
    work->Result = std::unexpected(std::move(publication.Message));
  } else if (work->RequestKey.empty()) {
    work->Result = std::optional<Product>(std::move(publication.Receipt));
  } else {
    work->Published = std::move(publication.Receipt);
    work->BindingQueued = true;
    const bool posted = Pool_.PostDetached([work, pages = Pages_] {
      work->Bound.emplace(pages->Prepare(*work->Request, nullptr, work->Stop.get_token()));
      work->Complete.store(true, std::memory_order_release);
    });
    if (posted) { return; }
    work->Result = std::unexpected("road page alias dispatcher stopped");
  }
  work->Complete.store(true, std::memory_order_release);
}

void RoadPageQueue::CompleteBinding(const std::shared_ptr<Work> &work) {
  const auto publication = work->Ticket->Poll();
  if (publication.Status == AssetPublicationStatus::Cancelled || work->Stop.stop_requested()) {
    work->Result = std::unexpected("road page request canceled");
  } else if (!*work->Bound) {
    work->Result = std::unexpected(std::move(work->Bound->error()));
  } else {
    auto stored =
        std::make_shared<BoundReceipt>(BoundReceipt{.Record = std::move(work->Bound->value()),
                                                    .Publication = std::move(work->Published),
                                                    .Retention = std::move(*work->Ticket)});
    Receipt receipt(stored, &stored->Record);
    work->Result = std::optional<Product>(std::move(receipt));
  }
  work->Bound.reset();
}
}

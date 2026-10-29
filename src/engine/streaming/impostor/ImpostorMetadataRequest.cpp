#include "ImpostorMetadataRequest.h"
#include <atomic>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
namespace {
using Metadata = Content::ImpostorViewAssets;
}

struct ImpostorMetadataRequest::State {
  std::shared_ptr<AssetCache> Cache;
  std::string Provenance;
  AssetPipeline::RequestTicket Ticket;
  std::shared_ptr<const AssetRecord> Receipt;
  std::stop_source Stop;
  std::atomic<bool> Complete{false};
  enum class Phase { Publication, Read };
  Phase Current = Phase::Publication;
  std::optional<Metadata> Value;
  std::string Error;

  State(std::shared_ptr<AssetCache> cache,
        std::string provenance,
        AssetPipeline::RequestTicket ticket)
      : Cache(std::move(cache)), Provenance(std::move(provenance)), Ticket(std::move(ticket)) {}

  [[nodiscard]] bool Cancelled() const noexcept {
    return Stop.stop_requested() || Ticket.Poll().Status == AssetPublicationStatus::Cancelled;
  }

  std::expected<Metadata, std::string> Read() const {
    if (Cancelled()) { return std::unexpected("impostor metadata request cancelled"); }
    auto opened = Metadata::Open(*Cache, Provenance);
    if (!opened || !*opened) { return std::unexpected("published impostor metadata unavailable"); }
    if (!Receipt || (**opened).Receipt().Package != Receipt->Package) {
      return std::unexpected("impostor metadata changed before loading");
    }
    if (Cancelled()) { return std::unexpected("impostor metadata request cancelled"); }
    return std::move(**opened);
  }
};

ImpostorMetadataRequest::ImpostorMetadataRequest(Tasks &workers,
                                                 std::shared_ptr<State> state) noexcept
    : Workers_(&workers), State_(std::move(state)) {}

ImpostorMetadataRequest::ImpostorMetadataRequest(ImpostorMetadataRequest &&other) noexcept =
    default;

ImpostorMetadataRequest &
ImpostorMetadataRequest::operator=(ImpostorMetadataRequest &&other) noexcept {
  if (this != &other) {
    Cancel();
    Workers_ = other.Workers_;
    State_ = std::move(other.State_);
  }
  return *this;
}

ImpostorMetadataRequest::~ImpostorMetadataRequest() {
  Cancel();
}

std::expected<ImpostorMetadataRequest, AssetPublicationError>
ImpostorMetadataRequest::Start(AssetPipeline &pipeline,
                               const AssetPipeline::Consumer &consumer,
                               Tasks &workers,
                               std::shared_ptr<AssetCache> cache,
                               std::string provenance,
                               Resolver resolve,
                               size_t inputBytes) {
  constexpr size_t fixedConsumerBytes = sizeof(State) + 3 * Metadata::MetadataBytesMost;
  if (!cache || provenance.empty() || provenance.size() > Metadata::MetadataBytesMost || !resolve ||
      provenance.capacity() > std::numeric_limits<size_t>::max() - fixedConsumerBytes) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  const auto key = Metadata::MetadataKey(provenance);
  const auto product = AssetProductKey::Parse(key);
  if (!product) { return std::unexpected(AssetPublicationError::InvalidRequest); }
  auto produce =
      [resolve = std::move(resolve),
       key](const std::stop_token &stop) -> std::expected<std::optional<AssetRecord>, std::string> {
    if (stop.stop_requested()) { return std::unexpected("impostor metadata request cancelled"); }
    auto result = resolve(stop);
    if (!result) { return std::unexpected(result.error()); }
    if (!*result) { return std::optional<AssetRecord>{}; }
    if (stop.stop_requested()) { return std::unexpected("impostor metadata request cancelled"); }
    const auto &receipt = (**result).Receipt();
    if (receipt.Key != key || receipt.ByteCount > Metadata::MetadataBytesMost) {
      return std::unexpected("impostor metadata producer returned another address or invalid size");
    }
    return receipt;
  };
  auto ticket =
      pipeline.RequestResumable(consumer,
                                {.Key = *product,
                                 .Generation = consumer.Generation(),
                                 .InputBytes = inputBytes,
                                 .ConsumerBytes = fixedConsumerBytes + provenance.capacity(),
                                 .ReceiptBytesMost = Metadata::MetadataBytesMost},
                                std::move(produce));
  if (!ticket) { return std::unexpected(ticket.error()); }
  return ImpostorMetadataRequest(
      workers,
      std::make_shared<State>(std::move(cache), std::move(provenance), std::move(*ticket)));
}

std::expected<std::shared_ptr<const Metadata>, std::string> ImpostorMetadataRequest::Poll() {
  if (!State_ || State_->Cancelled()) {
    return std::unexpected("impostor metadata request cancelled");
  }
  if (State_->Current == State::Phase::Publication) {
    const auto publication = State_->Ticket.Poll();
    if (!publication.Finished) { return std::shared_ptr<const Metadata>{}; }
    if (publication.Status != AssetPublicationStatus::Ready || !publication.Receipt) {
      return std::unexpected(publication.Message.empty() ? "impostor metadata publication failed"
                                                         : publication.Message);
    }
    State_->Receipt = publication.Receipt;
    State_->Current = State::Phase::Read;
    if (!Workers_->PostDetached([state = State_] {
          auto loaded = state->Read();
          if (loaded) {
            state->Value.emplace(std::move(*loaded));
          } else {
            state->Error = std::move(loaded.error());
          }
          state->Complete.store(true, std::memory_order_release);
        })) {
      State_->Error = "impostor metadata loader unavailable";
      State_->Complete.store(true, std::memory_order_release);
    }
  }
  if (!State_->Complete.load(std::memory_order_acquire)) {
    return std::shared_ptr<const Metadata>{};
  }
  if (!State_->Value) { return std::unexpected(State_->Error); }
  return std::shared_ptr<const Metadata>(State_, &*State_->Value);
}

void ImpostorMetadataRequest::Cancel() noexcept {
  if (auto state = std::move(State_)) {
    (void)state->Stop.request_stop();
    state->Ticket.Cancel();
  }
}
}

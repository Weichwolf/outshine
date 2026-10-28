#include "ImpostorFieldLoad.h"
#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <stop_token>
#include <utility>

namespace outshine {
struct ImpostorFieldLoad::State {
  std::shared_ptr<AssetCache> Cache;
  std::string Provenance, ProductKey;
  Content::ImpostorViewAssets::ViewIndex View;
  size_t BytesMost = 0;
  AssetPipeline::RequestTicket Ticket;
  std::stop_source Stop;
  std::atomic<bool> Complete{false};
  std::expected<ImpostorAtlas, std::string> Result = std::unexpected("field is not ready");

  State(std::shared_ptr<AssetCache> cache,
        std::string provenance,
        std::string productKey,
        Content::ImpostorViewAssets::ViewIndex view,
        size_t bytesMost,
        AssetPipeline::RequestTicket ticket)
      : Cache(std::move(cache)),
        Provenance(std::move(provenance)),
        ProductKey(std::move(productKey)),
        View(view),
        BytesMost(bytesMost),
        Ticket(std::move(ticket)) {}

  std::expected<ImpostorAtlas, std::string> Load() const {
    if (Stop.stop_requested() || Ticket.Poll().Status == AssetPublicationStatus::Cancelled) {
      return std::unexpected("impostor field load cancelled");
    }
    const auto opened = Content::ImpostorViewAssets::Open(*Cache, Provenance);
    if (!opened || !*opened) { return std::unexpected("impostor field metadata unavailable"); }
    const auto &views = **opened;
    if (views.FieldKey(View) != ProductKey) {
      return std::unexpected("impostor field metadata changed before loading");
    }
    auto loaded = views.LoadField(View, BytesMost);
    if (!loaded || !*loaded) { return std::unexpected("published impostor field unavailable"); }
    if (Stop.stop_requested() || Ticket.Poll().Status == AssetPublicationStatus::Cancelled) {
      return std::unexpected("impostor field load cancelled");
    }
    return std::move(**loaded);
  }
};

ImpostorFieldLoad::ImpostorFieldLoad(std::shared_ptr<State> state) noexcept
    : State_(std::move(state)) {}

ImpostorFieldLoad::ImpostorFieldLoad(ImpostorFieldLoad &&) noexcept = default;

ImpostorFieldLoad &ImpostorFieldLoad::operator=(ImpostorFieldLoad &&other) noexcept {
  if (this != &other) {
    Cancel();
    State_ = std::move(other.State_);
  }
  return *this;
}

ImpostorFieldLoad::~ImpostorFieldLoad() {
  Cancel();
}

std::expected<ImpostorFieldLoad, std::string>
ImpostorFieldLoad::Start(Tasks &workers,
                         std::shared_ptr<AssetCache> cache,
                         std::string provenance,
                         Content::ImpostorViewAssets::ViewIndex view,
                         size_t bytesMost,
                         AssetPipeline::RequestTicket ticket) {
  const auto publication = ticket.Poll();
  if (!cache || provenance.empty() || bytesMost == 0 || !publication.Finished ||
      publication.Status != AssetPublicationStatus::Ready || !publication.Receipt) {
    return std::unexpected("impostor field load requires a ready publication");
  }
  if (publication.Receipt->ByteCount > bytesMost) {
    return std::unexpected("impostor field load exceeds its encoded byte limit");
  }
  auto state = std::make_shared<State>(std::move(cache),
                                       std::move(provenance),
                                       publication.Receipt->Key,
                                       view,
                                       bytesMost,
                                       std::move(ticket));
  if (!workers.PostDetached([state] {
        state->Result = state->Load();
        state->Complete.store(true, std::memory_order_release);
      })) {
    return std::unexpected("impostor field loader unavailable");
  }
  return ImpostorFieldLoad(std::move(state));
}

std::expected<std::shared_ptr<const ImpostorAtlas>, std::string> ImpostorFieldLoad::Poll() const {
  if (!State_ || State_->Stop.stop_requested() ||
      State_->Ticket.Poll().Status == AssetPublicationStatus::Cancelled) {
    return std::unexpected("impostor field load cancelled");
  }
  if (!State_->Complete.load(std::memory_order_acquire)) {
    return std::shared_ptr<const ImpostorAtlas>{};
  }
  if (!State_->Result) { return std::unexpected(State_->Result.error()); }
  return std::shared_ptr<const ImpostorAtlas>(State_, &*State_->Result);
}

void ImpostorFieldLoad::Cancel() noexcept {
  if (State_) {
    (void)State_->Stop.request_stop();
    State_->Ticket.Cancel();
  }
}
}

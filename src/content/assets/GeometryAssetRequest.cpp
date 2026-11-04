#include "GeometryAssetRequest.h"
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
std::expected<AssetProduct, std::string>
PublishGeometry(const AssetCache &cache,
                const std::string &key,
                const std::string &parent,
                GeometryAssetRequest::Budget budget,
                const GeometryAssetRequest::Publisher &publish,
                const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("geometry publication cancelled"); }
  auto layout = GeometryAsset::Describe(cache, key, parent, budget.EncodedBytesMost);
  auto record = cache.Find(key);
  if (!layout || !record) { return std::unexpected("geometry publication lookup failed"); }
  if (!*layout || !*record) {
    if (!publish || budget.ProducerBytes == 0) {
      return std::unexpected("geometry asset unavailable");
    }
    const auto generated = publish(stop);
    if (!generated) { return std::unexpected(generated.error()); }
    layout = GeometryAsset::Describe(cache, key, parent, budget.EncodedBytesMost);
    record = cache.Find(key);
  }
  if (stop.stop_requested()) { return std::unexpected("geometry publication cancelled"); }
  if (!layout || !*layout || !record || !*record) {
    return std::unexpected("geometry publisher returned no complete product");
  }
  auto receipt = std::move(**record);
  if (receipt.Key != key || receipt.Parent != parent || receipt.Kind != "native-geometry" ||
      receipt.ByteCount > budget.EncodedBytesMost ||
      receipt.Bounds != (**layout).Bounds.Through((**layout).WorldFromLocal)) {
    return std::unexpected("geometry publisher returned an incomplete or foreign product");
  }
  return AssetProduct(std::move(receipt));
}
}

struct GeometryAssetRequest::State {
  enum class Phase { Publication, Read, Ready };
  std::shared_ptr<AssetCache> Cache;
  std::string Key;
  std::string Parent;
  Budget Limits;
  AssetPipeline::Consumer Consumer;
  AssetPipeline::RequestTicket Ticket;
  Preparation Prepare;
  std::shared_ptr<const void> PreparationData;
  std::shared_ptr<const AssetRecord> Receipt;
  std::stop_source Stop;
  Phase Current = Phase::Publication;
  std::atomic_bool Complete{false};
  size_t ResidentBytes = 0;
  std::expected<GeometryAsset::Product, std::string> Value = std::unexpected("geometry not ready");

  State(std::shared_ptr<AssetCache> cache,
        std::string key,
        std::string parent,
        Budget budget,
        AssetPipeline::Consumer consumer,
        AssetPipeline::RequestTicket ticket,
        Preparation prepare)
      : Cache(std::move(cache)),
        Key(std::move(key)),
        Parent(std::move(parent)),
        Limits(budget),
        Consumer(std::move(consumer)),
        Ticket(std::move(ticket)),
        Prepare(std::move(prepare)) {}

  [[nodiscard]] bool Cancelled() const noexcept {
    return Stop.stop_requested() || Ticket.Poll().Status == AssetPublicationStatus::Cancelled;
  }

  std::expected<GeometryAsset::Product, std::string> Read() {
    if (Cancelled()) { return std::unexpected("geometry load cancelled"); }
    auto loaded = GeometryAsset::Load(*Cache, Key, Parent, Limits.EncodedBytesMost);
    const auto record = Cache->Find(Key);
    if (!loaded || !*loaded || !record || !*record || !Receipt || **record != *Receipt) {
      (void)Ticket.Reject();
      return std::unexpected("published geometry changed or became unavailable before loading");
    }
    const size_t fixed = sizeof(State) + Key.capacity() + Parent.capacity() + 2;
    const auto retained = [&](size_t extra) -> std::optional<size_t> {
      const size_t mesh = (**loaded).Mesh.storageBytes();
      const auto &coverage = (**loaded).Layout.Coverage;
      const size_t metadata = coverage ? coverage->CapacityBytes() : 0;
      if (fixed > Limits.ConsumerBytes || mesh > Limits.ConsumerBytes - fixed ||
          metadata > Limits.ConsumerBytes - fixed - mesh ||
          extra > Limits.ConsumerBytes - fixed - mesh - metadata) {
        return std::nullopt;
      }
      return fixed + mesh + metadata + extra;
    };
    if (!retained(0)) { return std::unexpected("decoded geometry exceeds consumer admission"); }
    if (Cancelled()) { return std::unexpected("geometry load cancelled"); }
    size_t extra = 0;
    if (Prepare) {
      const auto prepared = Prepare(**loaded);
      if (!prepared) { return std::unexpected(prepared.error()); }
      if (static_cast<bool>(prepared->Owner) != (prepared->Bytes != 0)) {
        return std::unexpected("geometry preparation needs an owner for retained bytes");
      }
      extra = prepared->Bytes;
      PreparationData = prepared->Owner;
    }
    const auto bytes = retained(extra);
    if (!bytes) { return std::unexpected("prepared geometry exceeds consumer admission"); }
    ResidentBytes = *bytes;
    if (Cancelled()) { return std::unexpected("geometry load cancelled"); }
    return std::move(**loaded);
  }
};

GeometryAssetRequest::GeometryAssetRequest(Tasks &workers, std::shared_ptr<State> state) noexcept
    : Workers_(&workers), State_(std::move(state)) {}

GeometryAssetRequest::GeometryAssetRequest(GeometryAssetRequest &&) noexcept = default;

GeometryAssetRequest &GeometryAssetRequest::operator=(GeometryAssetRequest &&other) noexcept {
  if (this != &other) {
    Cancel();
    Workers_ = other.Workers_;
    State_ = std::move(other.State_);
  }
  return *this;
}

GeometryAssetRequest::~GeometryAssetRequest() {
  Cancel();
}

std::expected<GeometryAssetRequest, AssetPublicationError>
GeometryAssetRequest::Start(AssetPipeline &pipeline,
                            Tasks &workers,
                            std::shared_ptr<AssetCache> cache,
                            std::string key,
                            std::string parent,
                            Budget budget,
                            Publisher publish,
                            Preparation prepare) {
  const auto product = AssetProductKey::Parse(key);
  if (!cache || !product || (!parent.empty() && !AssetProductKey::Parse(parent)) ||
      budget.EncodedBytesMost == 0 || budget.ConsumerBytes < sizeof(State) + 2 ||
      budget.ReceiptBytesMost == 0) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  const size_t room = budget.ConsumerBytes - sizeof(State) - 2;
  if (key.capacity() > room || parent.capacity() > room - key.capacity()) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  if (!consumer) { return std::unexpected(consumer.error()); }
  auto ticket = pipeline.RequestResolved(
      *consumer,
      {.Key = *product,
       .Generation = consumer->Generation(),
       .InputBytes = budget.ProducerBytes,
       .ConsumerBytes = budget.ConsumerBytes,
       .ReceiptBytesMost = budget.ReceiptBytesMost},
      [cache, key, parent, budget, publish = std::move(publish)](const std::stop_token &stop) {
        return PublishGeometry(*cache, key, parent, budget, publish, stop);
      });
  if (!ticket) { return std::unexpected(ticket.error()); }
  return GeometryAssetRequest(workers,
                              std::make_shared<State>(std::move(cache),
                                                      std::move(key),
                                                      std::move(parent),
                                                      budget,
                                                      std::move(*consumer),
                                                      std::move(*ticket),
                                                      std::move(prepare)));
}

std::expected<std::shared_ptr<const GeometryAsset::Product>, std::string>
GeometryAssetRequest::Poll() {
  if (!State_ || State_->Cancelled()) { return std::unexpected("geometry request cancelled"); }
  if (State_->Current == State::Phase::Publication) {
    const auto publication = State_->Ticket.Poll();
    if (!publication.Finished) { return std::shared_ptr<const GeometryAsset::Product>{}; }
    if (publication.Status != AssetPublicationStatus::Ready || !publication.Receipt) {
      return std::unexpected(publication.Message.empty() ? "geometry publication failed"
                                                         : publication.Message);
    }
    State_->Receipt = publication.Receipt;
    State_->Current = State::Phase::Read;
    if (!Workers_->PostDetached([state = State_] {
          state->Value = state->Read();
          state->Prepare = {};
          state->Complete.store(true, std::memory_order_release);
        })) {
      State_->Value = std::unexpected("geometry worker unavailable");
      State_->Complete.store(true, std::memory_order_release);
    }
  }
  if (!State_->Complete.load(std::memory_order_acquire)) {
    return std::shared_ptr<const GeometryAsset::Product>{};
  }
  if (!State_->Value) { return std::unexpected(State_->Value.error()); }
  if (State_->Current == State::Phase::Read) {
    const auto retained = State_->Ticket.SetConsumerBytes(State_->ResidentBytes);
    if (!retained) { return std::unexpected("geometry residency admission unavailable"); }
    State_->Current = State::Phase::Ready;
  }
  return std::shared_ptr<const GeometryAsset::Product>(State_, &*State_->Value);
}

void GeometryAssetRequest::Cancel() noexcept {
  if (State_) {
    (void)State_->Stop.request_stop();
    State_->Ticket.Cancel();
  }
}
}

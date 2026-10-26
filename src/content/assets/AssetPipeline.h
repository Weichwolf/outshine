#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPIPELINE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPIPELINE_H
#include "AssetPublication.h"
#include "Tasks.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>

namespace outshine {
class AssetPipeline {
  struct State;

public:
  struct Limits {
    size_t JobsMost = 0;
    size_t ConsumersMost = 0;
    size_t TicketsMost = 0;
    size_t InputBytesMost = 0;
    size_t ReceiptBytesMost = 0;
  };

  struct Snapshot {
    size_t Jobs = 0;
    size_t Running = 0;
    size_t Consumers = 0;
    size_t Tickets = 0;
    size_t InputBytes = 0;
    size_t ReceiptBytes = 0;
  };

  class Consumer {
  public:
    Consumer(Consumer &&other) noexcept;
    Consumer &operator=(Consumer &&other) noexcept;
    ~Consumer();
    Consumer(const Consumer &) = delete;
    Consumer &operator=(const Consumer &) = delete;

    [[nodiscard]] AssetConsumerId Id() const noexcept { return Id_; }

    [[nodiscard]] AssetConsumerGeneration Generation() const noexcept;
    [[nodiscard]] bool Advance(AssetConsumerGeneration generation) noexcept;

  private:
    friend class AssetPipeline;
    Consumer(std::shared_ptr<State> state, AssetConsumerId id);
    std::shared_ptr<State> State_;
    AssetConsumerId Id_;
  };

  class RequestTicket {
  public:
    RequestTicket(RequestTicket &&other) noexcept;
    RequestTicket &operator=(RequestTicket &&other) noexcept;
    ~RequestTicket();
    RequestTicket(const RequestTicket &) = delete;
    RequestTicket &operator=(const RequestTicket &) = delete;
    void Cancel() noexcept;
    [[nodiscard]] AssetPublication Poll() const noexcept;

  private:
    friend class AssetPipeline;
    RequestTicket(std::shared_ptr<State> state, uint64_t id);
    std::shared_ptr<State> State_;
    uint64_t Id_ = 0;
  };

  struct ResolvedRequest {
    AssetProductKey Key;
    AssetConsumerGeneration Generation;
    size_t InputBytes = 0;
    size_t ConsumerBytes = 0;
    size_t ReceiptBytesMost = 0;
  };

  using Producer = std::function<std::expected<AssetRecord, std::string>(std::stop_token)>;
  AssetPipeline(Tasks &pool, Limits limits);
  [[nodiscard]] std::expected<Consumer, AssetPublicationError>
  OpenConsumer(AssetStoreScope scope = {});
  [[nodiscard]] std::expected<RequestTicket, AssetPublicationError>
  RequestResolved(const Consumer &consumer, const ResolvedRequest &request, Producer producer);
  [[nodiscard]] Snapshot Counts() const noexcept;

private:
  std::shared_ptr<State> State_;
};
}
#endif

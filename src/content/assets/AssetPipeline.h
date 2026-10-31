#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPIPELINE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPIPELINE_H
#include "AssetPublication.h"
#include "AssetProduct.h"
#include "Tasks.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>

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
    size_t Addresses = 0;
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
    // Reject a completed receipt for its joined demands; new requests resolve again.
    [[nodiscard]] bool Reject() noexcept;
    [[nodiscard]] AssetPublication Poll() const noexcept;
    // Replace this ticket's consumer retention before allocating owner buffers.
    // Release old buffers before shrinking; cancellation retains bytes until ticket destruction.
    [[nodiscard]] std::expected<void, AssetPublicationError>
    SetConsumerBytes(size_t bytes) noexcept;

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

  struct LogicalRequest {
    // Address binds the product recipe and immutable input scope.
    std::string_view Address;
    AssetConsumerGeneration Generation;
    size_t InputBytes = 0;
    size_t ConsumerBytes = 0;
    size_t ReceiptBytesMost = 0;
  };

  // Native backings retain their portion of InputBytes through the last published alias.
  // ReceiptBytesMost bounds metadata; one shared RAM bound covers inputs and held backings.

  using Producer = std::function<std::expected<AssetProduct, std::string>(std::stop_token)>;
  // An empty successful step retains admission and resumes after a bounded delay on Poll.
  // The pool must outlive every unfinished job, including deferred jobs.
  using ResumableProducer =
      std::function<std::expected<std::optional<AssetProduct>, std::string>(std::stop_token)>;

  // The next call starts only after this total input bound replaces the current one.
  // The producer releases old stage buffers before requesting a smaller bound.
  struct InputAdmission {
    size_t Bytes = 0;
  };

  struct LogicalStep;
  using LogicalProducer = std::function<std::expected<LogicalStep, std::string>(std::stop_token)>;

  struct ResolvedWork {
    AssetProductKey Key;
    LogicalProducer Produce;
  };

  struct LogicalStep : std::variant<std::optional<AssetProduct>, ResolvedWork, InputAdmission> {
    using variant::variant;
  };

  AssetPipeline(Tasks &pool, Limits limits);
  [[nodiscard]] std::expected<Consumer, AssetPublicationError>
  OpenConsumer(AssetStoreScope scope = {});
  [[nodiscard]] std::expected<RequestTicket, AssetPublicationError>
  RequestResolved(const Consumer &consumer, const ResolvedRequest &request, Producer producer);
  [[nodiscard]] std::expected<RequestTicket, AssetPublicationError> RequestResumable(
      const Consumer &consumer, const ResolvedRequest &request, ResumableProducer producer);
  // Address is borrowed only during submission; jobs retain its parsed digest.
  // A hit returns its bound record; a miss returns ResolvedWork after input resolution.
  // ResolvedWork inherits the admitted input/output bounds and never submits a child job.
  // Each ticket retains its address; all resolved consumers share an unannotated receipt.
  [[nodiscard]] std::expected<RequestTicket, AssetPublicationError>
  RequestLogical(const Consumer &consumer, const LogicalRequest &request, LogicalProducer producer);
  // Move a completed ticket's consumer bytes onto its existing consumer.
  // Destroy retained buffers before that consumer; releasing the ticket may free its native
  // package.
  [[nodiscard]] static std::expected<void, AssetPublicationError>
  RetainConsumerBytes(const Consumer &consumer, RequestTicket &ticket) noexcept;
  [[nodiscard]] Snapshot Counts() const noexcept;

private:
  enum class KeyDomain : uint8_t { Content, Request };

  struct Submission {
    AssetProductKey Key;
    KeyDomain Domain;
    AssetConsumerGeneration Generation;
    size_t InputBytes = 0;
    size_t ConsumerBytes = 0;
    size_t ReceiptBytesMost = 0;
  };

  [[nodiscard]] std::expected<RequestTicket, AssetPublicationError>
  Submit(const Consumer &consumer, const Submission &request, LogicalProducer producer);
  std::shared_ptr<State> State_;
};
}
#endif

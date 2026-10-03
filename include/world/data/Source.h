#ifndef OUTSHINE_WORLD_DATA_SOURCE_H
#define OUTSHINE_WORLD_DATA_SOURCE_H

#include <cstdint>
#include <expected>
#include <span>
#include <scene/HeightRaster.h>

#include "Fetched.h"
#include "Fetch.h"
#include "SourceDecl.h"
#include "Transport.h"

namespace outshine::Data {

/// Spatial/category admission, independent of whether a source has downloaded data.
enum class Coverage : uint8_t {
  Inside, ///< Source can attempt this request; not a readiness guarantee.
  Outside ///< Request is outside this source's domain.
};

/// Native decoding failure; an unsupported category is distinct from corrupt source bytes.
enum class DecodeFailure : uint8_t {
  Unsupported,   ///< Source does not provide this native product.
  CorruptPayload ///< Source bytes cannot produce a valid native product.
};

/// Owned configured source, queried on IO workers through engine scheduling.
/// Declaration and spatial methods must not perform IO. Concurrent calls are possible;
/// implementations synchronize mutable state. SourceSet owns each configured source;
/// the source copies factory inputs and must not retain borrowed call arguments.
/// Begin starts bounded work; Collect polls remote work or completes bounded local IO.
/// Neither runs in the frame path. Received source bytes
/// retain declared encoding and identity; the engine owns their persistent cache.
class Source {
public:
  /// Destroy only after all queries are settled or cancelled and workers have stopped.
  virtual ~Source() = default;

  /// Return immutable metadata owned by this source for its complete lifetime.
  /// @return Borrowed declaration; constant-time and no allocation or IO.
  [[nodiscard]] virtual const SourceDecl &Declaration() const noexcept = 0;

  /// Admit a category/address without IO or allocation.
  /// @param request Borrowed request, valid for this call.
  /// @return Inside or Outside; admission does not prove presence.
  [[nodiscard]] virtual Coverage Covers(const Fetch &request) const noexcept = 0;

  /// Resolve admitted demand to the actual source address, including ancestor selection.
  /// @param request Borrowed admitted request.
  /// @return Source address used by cache and provenance; no IO or allocation.
  [[nodiscard]] virtual Address Serves(const Fetch &request) const noexcept = 0;

  /// Start bounded IO or provider work on an IO worker.
  /// @param at Admitted source address, copied if retained.
  /// @param transport Borrowed transport used until completion or cancellation.
  /// @return Owned ticket or start failure; None is valid only for immediately collectable work.
  [[nodiscard]] virtual FetchStart Begin(const Address &at, Transport &transport) const = 0;

  /// Start admitted source demand including its interval and revision pin on an IO worker.
  /// Whole-payload providers inherit forwarding; partial demand requires an override.
  /// @param request Borrowed demand at Serves' resolved address; copy retained values.
  /// @param transport Transport retained until completion or cancellation.
  /// @return Owned ticket or refusal; default rejects unsupported partial demand.
  [[nodiscard]] virtual FetchStart Begin(const Fetch &request, Transport &transport) const {
    if (request.Range()) { return std::unexpected(FetchFailureReason::ProviderRefused); }
    return Begin(request.Where(), transport);
  }

  /// Poll remote work or complete bounded local IO on an IO worker; transfer payload ownership.
  /// @param at Address passed to Begin.
  /// @param ticket Active ticket owned by this query until terminal completion.
  /// @param transport Transport passed to Begin.
  /// @return Working, source bytes, confirmed absence, retry, or refusal.
  [[nodiscard]] virtual Fetched
  Collect(const Address &at, Ticket ticket, Transport &transport) const = 0;

  /// Decode source bytes into owned native height samples on the compute worker.
  /// @param bytes Borrowed cached/network bytes, valid only during this call.
  /// @return Owned samples or failure; default has no elevation decoder.
  /// No IO, persistent generated cache or retained byte references; may allocate bounded scratch.
  [[nodiscard]] virtual std::expected<HeightRaster, DecodeFailure>
  DecodeElevation([[maybe_unused]] std::span<const uint8_t> bytes) const {
    return std::unexpected(DecodeFailure::Unsupported);
  }

  /// Cancel one unfinished query; overrides also release provider-owned work.
  /// @param ticket Active ticket returned by Begin.
  /// @param transport Transport passed to Begin; default delegates cancellation to it.
  virtual void Cancel(Ticket ticket, Transport &transport) const { transport.Cancel(ticket); }
};

}
#endif

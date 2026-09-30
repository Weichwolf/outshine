#ifndef OUTSHINE_WORLD_DATA_SOURCE_H
#define OUTSHINE_WORLD_DATA_SOURCE_H

#include <cstdint>

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

  /// Poll remote work or complete bounded local IO on an IO worker; transfer payload ownership.
  /// @param at Address passed to Begin.
  /// @param ticket Active ticket owned by this query until terminal completion.
  /// @param transport Transport passed to Begin.
  /// @return Working, source bytes, confirmed absence, retry, or refusal.
  [[nodiscard]] virtual Fetched
  Collect(const Address &at, Ticket ticket, Transport &transport) const = 0;

  /// Cancel one unfinished query; overrides also release provider-owned work.
  /// @param ticket Active ticket returned by Begin.
  /// @param transport Transport passed to Begin; default delegates cancellation to it.
  virtual void Cancel(Ticket ticket, Transport &transport) const { transport.Cancel(ticket); }
};

}
#endif

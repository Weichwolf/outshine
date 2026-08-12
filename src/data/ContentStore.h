#ifndef CONTENTSTORE_H
#define CONTENTSTORE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Address.h"
#include "SourceDecl.h"

namespace outshine::Data {

/* THE NAME A DELIVERY IS FILED UNDER: the provider's own hash, and hash = filename. It covers the
 * source's id, its declared VERSION and the address, so two sources cannot collide and one source
 * cannot serve its old bytes after it starts producing new ones — provided it raises that version.
 * That is the one failure this scheme cannot catch, and it belongs stated here rather than trusted. */
[[nodiscard]] std::string ContentKey(const SourceDecl &decl, const Address &at);

/* A DIRECTORY OF FILES AND NOTHING ELSE. No index, no sidecar, no manifest: the file system already
 * holds a name-to-bytes map with atomic rename, and a second one beside it is a second thing that
 * can disagree with the first. A name never appears before its bytes are complete, because a write
 * lands on a temporary and is renamed into place.
 *
 * WHETHER IT IS USED IS THE RUN'S DECISION; where it lives and how large it grows are this
 * library's. Cache-on and cache-off differ in timing and in nothing else. */
class ContentStore {
public:
  enum class Use { Off, On };

  struct Config {
    /* Empty selects the library default: a directory named for this library under the host's
     * temporary directory. A store in the tree would be a file nobody committed on purpose. */
    std::string Directory;
    Use Using = Use::On;
    /* 0 selects the library default cap. The sweep runs once at construction, oldest first. */
    size_t CapBytes = 0;
  };

  explicit ContentStore(const Config &config);

  ContentStore(const ContentStore &) = delete;
  ContentStore &operator=(const ContentStore &) = delete;

  [[nodiscard]] bool TryRead(const std::string &key, std::vector<uint8_t> *out) const;
  void Keep(const std::string &key, const uint8_t *data, size_t bytes);

  [[nodiscard]] const std::string &Directory() const noexcept { return Directory_; }
  [[nodiscard]] bool Enabled() const noexcept { return Using_ == Use::On; }

  struct Ledger {
    long long Hits = 0, Misses = 0, Writes = 0, WriteFailures = 0, Swept = 0;
    long long SweptBytes = 0;
  };
  [[nodiscard]] Ledger Counters() const;

private:
  std::string Directory_;
  Use Using_;
  size_t CapBytes_;
  /* TryRead is const and still keeps score: the counter is the instrument, not the state. */
  mutable std::atomic<long long> Hits_{0}, Misses_{0};
  std::atomic<long long> Writes_{0}, WriteFailures_{0};
  long long Swept_ = 0, SweptBytes_ = 0;
  std::atomic<uint64_t> TempSerial_{0};
};

} // namespace outshine::Data
#endif

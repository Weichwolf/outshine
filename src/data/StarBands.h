#ifndef STARBANDS_H
#define STARBANDS_H

#include <string>

#include "Source.h"

namespace outshine::Data {

/* THE STAR CATALOGUE AS A SOURCE, and it is the one that proves the contract is not a tile
 * interface: whole-world addressing, no upstream, no cache. Four incremental magnitude bands of
 * HYG-derived positions, 6 bytes per star, generated in this tree and carried in the library's
 * declared data — 53 KB total, a bounded natural constant rather than a position-specific tile.
 *
 * IT IS NEVER VACANT. A band that will not read is a defect in the installation, never a statement
 * about the sky, so this source has no absence at all and refuses instead. */
class StarBands : public Source {
public:
  /* The directory the bands are declared in. A source that guessed it would be a source whose
   * refusal cannot name what it looked for. */
  explicit StarBands(std::string directory);

  static constexpr uint32_t kBands = 4;

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }
  [[nodiscard]] Coverage Covers(const Request &request) const noexcept override;
  [[nodiscard]] Address Serves(const Request &request) const noexcept override {
    return request.Where();
  }
  [[nodiscard]] Ticket Begin(const Address &at, Transport &transport) const override;
  [[nodiscard]] Fetched Collect(const Address &at, Ticket ticket,
                                Transport &transport) const override;

  [[nodiscard]] const std::string &Directory() const noexcept { return Directory_; }

private:
  std::string Directory_;
  SourceDecl Decl_;
};

} // namespace outshine::Data
#endif

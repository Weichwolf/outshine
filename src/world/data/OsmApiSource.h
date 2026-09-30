#ifndef OUTSHINE_WORLD_DATA_OSMAPISOURCE_H
#define OUTSHINE_WORLD_DATA_OSMAPISOURCE_H

#include <world/data/Source.h>
#include <world/SourceProvider.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace outshine::Data {

class OsmApiSource final : public Source {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<OsmApiSource>, std::string>
  Create(const SourceProvider &provider, uint32_t region);

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const Fetch &request) const noexcept override;

  [[nodiscard]] Address Serves(const Fetch &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] FetchStart Begin(const Address &at, Transport &transport) const override;
  [[nodiscard]] Fetched
  Collect(const Address &at, Ticket ticket, Transport &transport) const override;

private:
  OsmApiSource(const SourceProvider &provider, SourceCoverage bounds, uint32_t region);
  SourceDecl Decl_;
  uint32_t Region_;
};

}
#endif

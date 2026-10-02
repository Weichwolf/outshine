#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_OSMAPISOURCE_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_OSMAPISOURCE_H

#include <world/data/Source.h>
#include <world/SourceProvider.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace outshine::Generators::Osm {

class ApiSource final : public Data::Source {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<ApiSource>, std::string>
  Create(const Data::SourceProvider &provider, uint32_t region);

  [[nodiscard]] const Data::SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Data::Coverage Covers(const Data::Fetch &request) const noexcept override;

  [[nodiscard]] Data::Address Serves(const Data::Fetch &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] Data::FetchStart Begin(const Data::Address &at,
                                       Data::Transport &transport) const override;
  [[nodiscard]] Data::Fetched
  Collect(const Data::Address &at, Data::Ticket ticket, Data::Transport &transport) const override;

private:
  ApiSource(const Data::SourceProvider &provider, uint32_t region);
  [[nodiscard]] bool Accepts(const Data::Address &at) const noexcept;
  Data::SourceDecl Decl_;
  std::optional<uint32_t> Region_;
};

}
#endif

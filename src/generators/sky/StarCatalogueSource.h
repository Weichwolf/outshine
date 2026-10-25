#ifndef OUTSHINE_GENERATORS_SKY_STARCATALOGUESOURCE_H
#define OUTSHINE_GENERATORS_SKY_STARCATALOGUESOURCE_H

#include <string>

#include <Source.h>

namespace outshine::Generators::Sky {

class StarCatalogueSource : public Data::Source {
public:
  StarCatalogueSource(std::string directory,
                      std::string revision,
                      Data::Rank order,
                      Data::AbsencePolicy absence);

  static constexpr uint32_t kBands = 4;

  [[nodiscard]] const Data::SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Data::Coverage Covers(const Data::SourceRequest &request) const noexcept override;

  [[nodiscard]] Data::Address Serves(const Data::SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] Data::FetchStart Begin(const Data::Address &at,
                                       Data::Transport &transport) const override;
  [[nodiscard]] Data::SourceResponse
  Collect(const Data::Address &at, Data::Ticket ticket, Data::Transport &transport) const override;

  [[nodiscard]] const std::string &Directory() const noexcept { return Directory_; }

private:
  std::string Directory_;
  Data::SourceDecl Decl_;
};

}
#endif

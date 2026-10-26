#ifndef OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_COPERNICUSDEMSOURCE_H
#define OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_COPERNICUSDEMSOURCE_H

#include <string>
#include <string_view>
#include <Source.h>

namespace outshine::Generators::Terrain {

class CopernicusDemSource final : public Data::Source {
public:
  static constexpr std::string_view Endpoint = "https://copernicus-dem-30m.s3.amazonaws.com/";
  static constexpr std::string_view Dataset = "copernicus-glo30";
  explicit CopernicusDemSource(std::string revision = {},
                               Data::Rank order = Data::Rank{0},
                               Data::AbsencePolicy absence = Data::AbsencePolicy::Fail,
                               std::string dataset = std::string(Dataset));

  [[nodiscard]] const Data::SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Data::Coverage Covers(const Data::SourceRequest &request) const noexcept override;

  [[nodiscard]] Data::Address Serves(const Data::SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] Data::FetchStart Begin(const Data::Address &at,
                                       Data::Transport &transport) const override;
  [[nodiscard]] Data::FetchStart Begin(const Data::SourceRequest &request,
                                       Data::Transport &transport) const override;
  [[nodiscard]] Data::SourceResponse
  Collect(const Data::Address &at, Data::Ticket ticket, Data::Transport &transport) const override;
  [[nodiscard]] std::string Url(const Data::Address &at) const;

private:
  Data::SourceDecl Decl_;
};

}
#endif

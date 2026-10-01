#ifndef OUTSHINE_WORLD_DATA_COPERNICUSDEM_H
#define OUTSHINE_WORLD_DATA_COPERNICUSDEM_H

#include <string>
#include <string_view>
#include <world/data/Source.h>

namespace outshine::Data {

class CopernicusDem final : public Source {
public:
  static constexpr std::string_view Endpoint = "https://copernicus-dem-30m.s3.amazonaws.com/";
  static constexpr std::string_view Dataset = "copernicus-glo30";
  explicit CopernicusDem(std::string revision = {},
                         Rank order = Rank{0},
                         AbsencePolicy absence = AbsencePolicy::Fail,
                         std::string dataset = std::string(Dataset));

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const Fetch &request) const noexcept override;

  [[nodiscard]] Address Serves(const Fetch &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] FetchStart Begin(const Address &at, Transport &transport) const override;
  [[nodiscard]] FetchStart Begin(const Fetch &request, Transport &transport) const override;
  [[nodiscard]] Fetched
  Collect(const Address &at, Ticket ticket, Transport &transport) const override;
  [[nodiscard]] std::string Url(const Address &at) const;

private:
  SourceDecl Decl_;
};

}
#endif

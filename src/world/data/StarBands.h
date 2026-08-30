#ifndef OUTSHINE_WORLD_DATA_STARBANDS_H
#define OUTSHINE_WORLD_DATA_STARBANDS_H

#include <string>

#include "Source.h"

namespace outshine::Data {

class StarBands : public Source {
public:
  explicit StarBands(std::string directory);

  static constexpr uint32_t kBands = 4;

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const Request &request) const noexcept override;

  [[nodiscard]] Address Serves(const Request &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] Ticket Begin(const Address &at, Transport &transport) const override;
  [[nodiscard]] Fetched
  Collect(const Address &at, Ticket ticket, Transport &transport) const override;

  [[nodiscard]] const std::string &Directory() const noexcept { return Directory_; }

private:
  std::string Directory_;
  SourceDecl Decl_;
};

} // namespace outshine::Data
#endif

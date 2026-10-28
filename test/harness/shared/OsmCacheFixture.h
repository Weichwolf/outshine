#ifndef OUTSHINE_TEST_OSMCACHEFIXTURE_H
#define OUTSHINE_TEST_OSMCACHEFIXTURE_H
#include "Source.h"
#include "SourceDecl.h"
#include "SourceRequest.h"
#include "SourceResponse.h"
#include "SourceTransport.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Test::OsmCache {
class NoTransport final : public Data::SourceTransport {
public:
  Data::FetchStart Begin(const std::string & /*url*/) override { return Data::Ticket::None; }

  Data::Wire Collect(Data::Ticket /*ticket*/) override { return Data::Wire::Never(); }

  void Cancel(Data::Ticket /*ticket*/) override {}
};

class VectorSource final : public Data::Source {
public:
  VectorSource(std::string revision, std::vector<uint8_t> bytes, std::atomic_uint &calls)
      : Decl_{}, Bytes_(std::move(bytes)), Calls_(calls) {
    Decl_.Id = "fixture-vector";
    Decl_.Revision = std::move(revision);
    Decl_.Kind = Data::DataKind::VectorMap;
    Decl_.Wire = Data::WireFormat::MapboxVectorTile;
    Decl_.Keeps = Data::Cacheability::Never;
  }

  [[nodiscard]] const Data::SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Data::Coverage
  Covers(const Data::SourceRequest & /*request*/) const noexcept override {
    return Data::Coverage::Inside;
  }

  [[nodiscard]] Data::Address Serves(const Data::SourceRequest &request) const noexcept override {
    return request.Where();
  }

  Data::FetchStart Begin(const Data::Address & /*at*/,
                         Data::SourceTransport & /*transport*/) const override {
    return Data::Ticket::None;
  }

  Data::SourceResponse Collect(const Data::Address & /*at*/,
                               Data::Ticket /*ticket*/,
                               Data::SourceTransport & /*transport*/) const override {
    ++Calls_;
    return Data::SourceResponse::Delivered(Bytes_);
  }

private:
  Data::SourceDecl Decl_;
  std::vector<uint8_t> Bytes_;
  std::atomic_uint &Calls_;
};
}
#endif

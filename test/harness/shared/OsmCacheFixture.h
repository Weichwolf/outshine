#ifndef OUTSHINE_TEST_OSMCACHEFIXTURE_H
#define OUTSHINE_TEST_OSMCACHEFIXTURE_H
#include "Source.h"
#include "SourceSession.h"
#include "SourceDecl.h"
#include "SourceRequest.h"
#include "SourceResponse.h"
#include "SourceTransport.h"
#include <atomic>
#include <array>
#include <expected>
#include <memory>
#include <optional>
#include <string_view>
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

class SourceFactory final : public Data::Provider {
public:
  explicit SourceFactory(std::unique_ptr<Data::Source> source) : Source_(std::move(source)) {}

  [[nodiscard]] std::string_view kind() const override { return "osm-cache-fixture"; }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider & /*declaration*/,
       std::string_view /*builtinDataDirectory*/) const override {
    if (!Source_) { return std::unexpected("fixture source already constructed"); }
    return std::move(Source_);
  }

private:
  mutable std::unique_ptr<Data::Source> Source_;
};

inline std::expected<std::shared_ptr<Data::SourceSession>, std::string>
OpenSession(std::unique_ptr<Data::Source> source) {
  if (!source) { return std::unexpected("fixture source is missing"); }
  const auto revision = source->Declaration().Revision;
  const SourceFactory factory(std::move(source));
  Data::ProviderRegistry registry;
  if (!registry.registerProvider(factory)) {
    return std::unexpected("fixture factory registration failed");
  }
  const std::array providers{Data::SourceProvider{.Kind = "osm-cache-fixture",
                                                  .Revision = revision,
                                                  .Dataset = {},
                                                  .Location = {},
                                                  .Endpoint = {},
                                                  .Coverage = std::nullopt,
                                                  .PayloadSha256 = {},
                                                  .Schema = {}}};
  return Data::SourceSession::Open(
      {.Directory = {}, .Using = Data::RawSourceCache::Use::Off}, providers, {}, registry);
}
}
#endif

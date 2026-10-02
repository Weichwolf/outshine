#include "OsmProvider.h"
#include "OsmApiSource.h"
#include "OsmXmlReader.h"
#include "ReadTextFile.h"
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
class LocalSource final : public Data::Source {
public:
  LocalSource(const Data::SourceProvider &provider, std::string_view root) {
    const std::filesystem::path location(provider.Location);
    Decl_.Id = provider.Dataset;
    Decl_.Revision = provider.Revision;
    Decl_.Endpoint =
        (location.is_absolute() ? location : std::filesystem::path(root) / location).string();
    Decl_.Kind = Data::DataKind::OriginalOsm;
    Decl_.How = Data::Scheme::WholeWorld;
    Decl_.Wire = Data::WireFormat::OsmXml;
    Decl_.Order = static_cast<Data::Rank>(provider.Priority);
    Decl_.OnAbsent = Data::AbsencePolicy::Fail;
    Decl_.Keeps = Data::Cacheability::Never;
    Decl_.Latency = Data::LatencyClass::Local;
    Decl_.MaximumPayloadBytes = Data::kMaxOsmXmlBytes;
    Decl_.PayloadSha256 = provider.PayloadSha256;
  }

  [[nodiscard]] const Data::SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Data::Coverage Covers(const Data::Fetch &request) const noexcept override {
    return request.Kind() == Data::DataKind::OriginalOsm && request.Where().Index() == 0
               ? Data::Coverage::Inside
               : Data::Coverage::Outside;
  }

  [[nodiscard]] Data::Address Serves(const Data::Fetch &request) const noexcept override {
    return request.Where();
  }

  Data::FetchStart Begin(const Data::Address &at,
                         [[maybe_unused]] Data::Transport &transport) const override {
    return at.Index() == 0 ? Data::FetchStart(Data::Ticket::None)
                           : std::unexpected(Data::FetchFailureReason::InvalidRequest);
  }

  Data::Fetched Collect(const Data::Address &at,
                        [[maybe_unused]] Data::Ticket ticket,
                        [[maybe_unused]] Data::Transport &transport) const override {
    if (at.Index() != 0) {
      return Data::Fetched::Meant(Data::Meaning::Refused, Data::FetchFailureReason::InvalidRequest);
    }
    auto text = ReadTextFile(Decl_.Endpoint, Data::kMaxOsmXmlBytes);
    if (!text) { return Data::Fetched::Meant(Data::Meaning::Refused); }
    return Data::Fetched::Delivered(std::vector<uint8_t>(text->begin(), text->end()));
  }

private:
  Data::SourceDecl Decl_;
};

}

std::expected<std::unique_ptr<Data::Source>, std::string>
Provider::make(const Data::SourceProvider &provider, std::string_view root) const {
  if (provider.Endpoint.empty()) { return std::make_unique<LocalSource>(provider, root); }
  auto made = ApiSource::Create(provider, 0);
  if (!made) { return std::unexpected(std::move(made.error())); }
  return std::move(*made);
}
}

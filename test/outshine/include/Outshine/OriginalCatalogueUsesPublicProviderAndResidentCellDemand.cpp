#include <Outshine.h>
#include "Check.h"
#include <atomic>
#include <memory>
#include <string>
#include <utility>

namespace {
using namespace outshine::Data;

class OriginalSource final : public Source {
public:
  OriginalSource(const SourceProvider &provider, std::atomic<int> &calls) : Calls_(calls) {
    Decl_.Id = provider.Dataset;
    Decl_.Revision = provider.Revision;
    Decl_.Kind = DataKind::OriginalOsm;
    Decl_.How = Scheme::GeodeticGrid;
    Decl_.Wire = WireFormat::OsmXml;
    Decl_.OnAbsent = AbsencePolicy::Fail;
    Decl_.Keeps = Cacheability::Never;
  }

  const SourceDecl &Declaration() const noexcept override { return Decl_; }

  Coverage Covers(const Fetch &request) const noexcept override {
    return request.Kind() == DataKind::OriginalOsm ? Coverage::Inside : Coverage::Outside;
  }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    ++Calls_;
    const std::string xml = "<osm version='0.6'><node id='1' lat='54.78' lon='9.43'>"
                            "<tag k='custom:unknown' v='retained'/></node></osm>";
    return Fetched::Delivered(std::vector<uint8_t>(xml.begin(), xml.end()));
  }

private:
  SourceDecl Decl_;
  std::atomic<int> &Calls_;
};

class OriginalProvider final : public Provider {
public:
  mutable std::atomic<int> Acquired{0};

  std::string_view kind() const override { return "osm"; }

  std::expected<std::unique_ptr<Source>, std::string> make(const SourceProvider &provider,
                                                           std::string_view) const override {
    return std::make_unique<OriginalSource>(provider, Acquired);
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  OriginalProvider provider;
  Scenario::Document document;
  document.Ground.Origin = {.LatitudeDeg = 54.781286, .LongitudeDeg = 9.433995};
  document.Ground.SightM = 1.0;
  document.Providers.push_back({.Kind = "osm",
                                .Revision = "fixture-r1",
                                .Missing = Data::MissingDataPolicy::Fail,
                                .Dataset = "public.original.fixture",
                                .Endpoint = "https://api.openstreetmap.org/api/0.6"});
  Engine engine;
  CHECK(engine.registerProvider(provider) && engine.setRoots({.Offline = true}) &&
            engine.declare(document) && engine.assemble(),
        "public original provider admits a position-based catalogue demand without renderer IO");
  CHECK(engine.preload(2.0) && engine.settled(WorldQuality::Refined) && provider.Acquired == 1,
        "one geographic source cell publishes through the native original loader");
  const int acquired = provider.Acquired.load();
  CHECK(engine.preload(0.1) && engine.advance() && provider.Acquired == acquired,
        "unchanged position reuses published source cells without another acquisition");
  return Report();
}

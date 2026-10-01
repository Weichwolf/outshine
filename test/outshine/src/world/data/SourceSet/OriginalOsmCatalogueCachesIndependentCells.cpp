#include "Check.h"
#include "DeclaredSources.h"
#include "OfflineTransport.h"
#include "OsmApiReader.h"
#include "OsmXmlReader.h"
#include "SourceProviderValidation.h"
#include "SourceSet.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
class ApiWire final : public outshine::Data::Transport {
public:
  std::vector<std::string> Urls;
  const std::string Xml = "<osm version='0.6'><node id='1' lat='54.78' lon='9.43'/>"
                          "<way id='10'><nd ref='1'/><tag k='man_made' v='chimney'/>"
                          "<tag k='height' v='53.5'/><tag k='custom:unknown' v='preserved'/></way>"
                          "<relation id='20'><member type='way' ref='10' role='outer'/>"
                          "<tag k='type' v='multipolygon'/></relation></osm>";

  outshine::Data::FetchStart Begin(const std::string &url) override {
    Urls.push_back(url);
    return static_cast<outshine::Data::Ticket>(Urls.size());
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(Xml.begin(), Xml.end()));
  }

  void Cancel(outshine::Data::Ticket) override {}
};
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-original-cells-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original cell-response store created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const SourceProvider provider{.Kind = "osm",
                                .Revision = "original-source-cells",
                                .Missing = MissingDataPolicy::Fail,
                                .Dataset = "openstreetmap.original",
                                .Endpoint = std::string(kOfficialOsmApi)};
  const std::array cells{GeoCellId{.Level = 9, .X = 269, .Y = 411},
                         GeoCellId{.Level = 9, .X = 270, .Y = 411}};
  ProviderRegistry registry;
  RegisterShippedProviders(registry);
  std::array<std::string, 2> keys;
  ApiWire network;
  OfflineTransport offline;
  for (int pass = 0; pass < 2; ++pass) {
    ContentStore store({.Directory = directory});
    SourceSet sources(store);
    auto made = MakeDeclaredSource(provider, ".", &registry);
    CHECK(made && (*made)->Declaration().How == Scheme::GeodeticGrid &&
              (*made)->Declaration().Wire == WireFormat::OsmXml &&
              (*made)->Declaration().MaximumPayloadBytes == kMaxOsmXmlBytes,
          "the registered built-in provider serves bounded original cells through the public "
          "source contract");
    if (!made) { break; }
    const Source *source = made->get();
    CHECK(sources.Add(std::move(*made)) == SourceSet::Registration::Accepted,
          "catalogue registration uses the same source registry");
    sources.Seal();
    Transport &wire = pass == 0 ? static_cast<Transport &>(network) : offline;
    for (size_t at = 0; at < cells.size(); ++at) {
      const Address address = Address::AtGeoCell(cells[at]);
      auto query = sources.Ask(Fetch(DataKind::OriginalOsm, address));
      const auto answer = sources.Collect(query, wire).Take();
      CHECK(answer && answer->At == address && answer->SourceId == provider.Dataset &&
                answer->SourceRevision == provider.Revision &&
                std::string(answer->Bytes.begin(), answer->Bytes.end()) == network.Xml,
            "original bytes and source-cell provenance survive cold acquisition and offline "
            "reopening");
      if (answer) {
        const std::string xml(answer->Bytes.begin(), answer->Bytes.end());
        const auto parsed =
            OsmXmlReader::Read(xml, {.DatasetId = provider.Dataset, .Revision = provider.Revision});
        CHECK(parsed && parsed->FindNode(1) && parsed->FindWay(10) &&
                  parsed->FindWay(10)->Tags.size() == 3 && parsed->FindRelation(20),
              "original object kinds and unknown properties remain available to native consumers");
      }
      const auto key = ContentKey(source->Declaration(), address);
      if (pass == 0) { keys[at] = key; }
      CHECK(keys[at] == key && key != ContentKey(source->Declaration(), Address::AtCell({54, 9})) &&
                key != ContentKey(source->Declaration(), Address::At({9, 269, 411})),
            "geographic source identity is stable and cannot alias DEM or render tiles");
    }
    for (const GeoCellId invalid :
         {GeoCellId{.Level = 8}, GeoCellId{.Level = 25}, GeoCellId{.Level = 9, .X = 512}}) {
      auto query = sources.Ask(Fetch(DataKind::OriginalOsm, Address::AtGeoCell(invalid)));
      CHECK(sources.Collect(query, wire).Where() == Delivery::State::Undeclared,
            "oversized and invalid cells fail admission before transport or cache IO");
    }
    CHECK(sources.Counters().FromStore == (pass == 0 ? 0 : 2),
          "offline cells use received bytes through the shared raw source store");
  }
  const std::vector<std::string> expectedUrls{
      "https://api.openstreetmap.org/api/0.6/map?bbox=9.140625,54.4921875,9.84375,54.84375",
      "https://api.openstreetmap.org/api/0.6/map?bbox=9.84375,54.4921875,10.546875,54.84375"};
  CHECK(keys[0] != keys[1] && network.Urls == expectedUrls,
        "adjacent original cells use separate bounded official queries and cache identities");
  ContentStore noCache({.Using = ContentStore::Use::Off});
  const auto global = ReadOsmApiRegion(provider, noCache, network, network.NowMs() + 1000, {});
  CHECK(!global && global.error().find("geographic cell demand") != std::string::npos &&
            network.Urls.size() == 2,
        "a catalogue cannot masquerade as a complete bounded regional source");
  auto invalid = provider;
  invalid.PayloadSha256.assign(64, 'a');
  CHECK(!ValidateSourceProviders(std::span(&invalid, 1)),
        "one payload digest cannot claim to pin every catalogue cell");
  invalid = provider;
  invalid.Revision = "sha256:" + std::string(64, 'a');
  CHECK(!ValidateSourceProviders(std::span(&invalid, 1)),
        "a legacy single-response pin cannot become a catalogue revision");
  invalid = provider;
  invalid.Endpoint = "https://tiles.versatiles.org/api/0.6";
  CHECK(!ValidateSourceProviders(std::span(&invalid, 1)),
        "catalogues cannot substitute an unofficial reduced source");
  std::error_code error;
  (void)std::filesystem::remove_all(directory, error);
  return Report();
}

#include "OsmChunkSetLoader.h"
#include "SourceProviderValidation.h"
#include "Sha256.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-original-pins-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original-source directory created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const std::array<std::string, 2> xml{
      "<osm version='0.6'><node id='1' lat='0' lon='0'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><tag k='building' v='church'/>"
      "<tag k='roof:shape' v='gabled'/><tag k='custom:unknown' v='preserved'/></way></osm>",
      "<osm version='0.6'><node id='2' lat='0' lon='1'/>"
      "<relation id='10'><member type='way' ref='10' role='outer'/>"
      "<tag k='type' v='multipolygon'/></relation></osm>"};
  std::array<SourceProvider, 2> providers;
  for (size_t i = 0; i < providers.size(); ++i) {
    providers[i] = {.Kind = "osm",
                    .Revision = "official-map-snapshot",
                    .Priority = static_cast<int>(i),
                    .Missing = MissingDataPolicy::Fail,
                    .Dataset = "openstreetmap.original",
                    .Location = std::to_string(i) + ".osm",
                    .Coverage = SourceCoverage{.WestDeg = static_cast<double>(i),
                                               .SouthDeg = 0,
                                               .EastDeg = static_cast<double>(i + 1),
                                               .NorthDeg = 1},
                    .PayloadSha256 = Sha256Hex(xml[i])};
    std::ofstream file(std::filesystem::path(directory) / providers[i].Location);
    file << xml[i];
    CHECK(file.good(), "original response bytes stored without rewriting");
  }
  CHECK(providers[0].PayloadSha256 != providers[1].PayloadSha256 &&
            ValidateSourceProviders(providers).has_value(),
        "distinct response hashes validate under one dataset revision");
  const auto source = outshine::Generators::Osm::ChunkSetLoader::Load(providers, directory);
  CHECK(source.has_value(), "separately pinned responses close cross-region references");
  if (source) {
    CHECK(source->Elements.SourceIdentity().Revision == "official-map-snapshot" &&
              source->Elements.FindWay(10) && source->Elements.FindRelation(10),
          "merged source retains dataset revision and typed object identities");
    const auto *way = source->Elements.FindWay(10);
    CHECK(way && way->Tags.size() == 3 &&
              std::ranges::any_of(way->Tags,
                                  [](const outshine::Generators::Osm::Tag &tag) {
                                    return tag.Key == "custom:unknown" && tag.Value == "preserved";
                                  }),
          "unknown and rendering tags survive the pinned merge");
    CHECK(source->Chunks.size() == 2 && source->Coverage.size() == 2 &&
              source->Chunks[0].PayloadSha256 == providers[0].PayloadSha256 &&
              source->Chunks[1].PayloadSha256 == providers[1].PayloadSha256 &&
              source->Chunks[0].PinVerified && source->Chunks[1].PinVerified,
          "published source retains each response digest and pin verification");
  }
  auto wrong = providers;
  wrong[1].PayloadSha256 = wrong[0].PayloadSha256;
  const auto refused = outshine::Generators::Osm::ChunkSetLoader::LoadRegion(wrong, directory);
  CHECK(!refused && refused.error().find("sha256 pin") != std::string::npos,
        "one wrong response pin prevents publication of the merged region");
  auto legacy = providers[0];
  legacy.Revision = "sha256:" + legacy.PayloadSha256;
  legacy.PayloadSha256.clear();
  const std::array legacyProviders{legacy};
  CHECK(ValidateSourceProviders(legacyProviders).has_value(), "legacy byte pin remains valid");
  const auto legacySource =
      outshine::Generators::Osm::ChunkSetLoader::LoadRegion(legacyProviders, directory);
  CHECK(legacySource && legacySource->Chunks.size() == 1 && legacySource->Chunks[0].PinVerified &&
            legacySource->Chunks[0].PayloadSha256 == providers[0].PayloadSha256,
        "legacy byte pins still verify unchanged original responses");
  auto unpinned = providers[0];
  unpinned.PayloadSha256.clear();
  const std::array unpinnedProviders{unpinned};
  const auto unpinnedSource =
      outshine::Generators::Osm::ChunkSetLoader::LoadRegion(unpinnedProviders, directory);
  CHECK(unpinnedSource && unpinnedSource->Chunks.size() == 1 &&
            !unpinnedSource->Chunks[0].PinVerified &&
            unpinnedSource->Chunks[0].PayloadSha256 == providers[0].PayloadSha256,
        "observed digest of an unpinned response does not claim pin verification");
  wrong = providers;
  wrong[1].Revision = "another-snapshot";
  CHECK(!ValidateSourceProviders(wrong), "response pins do not permit mixing dataset revisions");
  wrong = providers;
  wrong[1].PayloadSha256[0] = 'G';
  CHECK(!ValidateSourceProviders(wrong), "non-hex response digest is rejected");
  wrong = providers;
  wrong[1].Revision = "sha256:" + providers[0].PayloadSha256;
  CHECK(!ValidateSourceProviders(wrong), "contradictory legacy and response pins are rejected");
  wrong = providers;
  wrong[1].Kind = "terrain";
  CHECK(!ValidateSourceProviders(wrong), "response pins are reserved for original OSM chunks");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated original-source directory removed");
  return Report();
}

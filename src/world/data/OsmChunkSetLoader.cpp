#include "OsmChunkSetLoader.h"

#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "OsmXmlReader.h"
#include "ReadTextFile.h"

namespace outshine::Data {

namespace {

constexpr size_t kMaxChunkBytes = size_t{4} * 1024u * 1024u;
constexpr size_t kMaxTotalBytes = size_t{16} * 1024u * 1024u;
constexpr size_t kMaxInputElements = 1000000;

[[nodiscard]] double MillisecondsSince(std::chrono::steady_clock::time_point began) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
      .count();
}

[[nodiscard]] std::string ElementError(const OsmMergeError &error) {
  return "semantic OSM merge failed at source element " + std::to_string(error.Id) + " with code " +
         std::to_string(static_cast<int>(error.Code));
}

}

std::expected<OsmChunkSet, std::string>
OsmChunkSetLoader::Load(std::span<const SourceProvider> providers, std::string_view shippedRoot) {
  std::vector<OsmElements> chunks;
  std::vector<SourceCoverage> coverage;
  chunks.reserve(providers.size());
  coverage.reserve(providers.size());
  size_t bytes = 0;
  double readMs = 0.0;
  double parseMs = 0.0;
  for (const SourceProvider &provider : providers) {
    const std::filesystem::path location(provider.Location);
    const std::filesystem::path path =
        location.is_absolute() ? location : std::filesystem::path(shippedRoot) / location;
    const auto readAt = std::chrono::steady_clock::now();
    auto xml = ReadTextFile(path.string(), kMaxChunkBytes);
    readMs += MillisecondsSince(readAt);
    if (!xml) { return std::unexpected(std::move(xml.error())); }
    if (xml->size() > kMaxTotalBytes - bytes) {
      return std::unexpected("semantic OSM source exceeds the total byte budget");
    }
    bytes += xml->size();
    const auto parseAt = std::chrono::steady_clock::now();
    auto parsed =
        OsmXmlReader::Read(*xml, {.DatasetId = provider.Dataset, .Revision = provider.Revision});
    parseMs += MillisecondsSince(parseAt);
    if (!parsed) {
      return std::unexpected("semantic OSM source '" + provider.Location +
                             "' failed XML import with code " +
                             std::to_string(static_cast<int>(parsed.error())));
    }
    if (!provider.Coverage) {
      return std::unexpected("semantic OSM source '" + provider.Location +
                             "' has no declared coverage");
    }
    coverage.push_back(*provider.Coverage);
    chunks.push_back(std::move(*parsed));
  }

  const auto mergeAt = std::chrono::steady_clock::now();
  auto merged = OsmElements::Merge(chunks, kMaxInputElements);
  parseMs += MillisecondsSince(mergeAt);
  if (!merged) { return std::unexpected(ElementError(merged.error())); }
  if (const auto missing = merged->FirstMissingReference()) {
    return std::unexpected("semantic OSM source element " + std::to_string(missing->OwnerId) +
                           " refers to missing element " + std::to_string(missing->MissingId));
  }
  return OsmChunkSet{.Elements = std::move(*merged),
                     .Coverage = std::move(coverage),
                     .SourceBytes = bytes,
                     .ReadMs = readMs,
                     .ParseMs = parseMs};
}

}

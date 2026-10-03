#include "OsmChunkSetLoader.h"

#include <chrono>
#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <ratio>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "OsmXmlReader.h"
#include "ReadTextFile.h"
#include "Sha256.h"

namespace outshine::Generators::Osm {

namespace {

constexpr size_t kMaxTotalBytes = size_t{16} * 1024u * 1024u;
constexpr size_t kMaxInputElements = 1000000;
constexpr std::string_view kSha256PinPrefix = "sha256:";

[[nodiscard]] std::string_view PayloadDigest(const Data::SourceProvider &provider) noexcept {
  if (!provider.PayloadSha256.empty()) { return provider.PayloadSha256; }
  const std::string_view pin(provider.Revision);
  if (!pin.starts_with(kSha256PinPrefix)) { return {}; }
  return pin.substr(kSha256PinPrefix.size());
}

[[nodiscard]] double MillisecondsSince(std::chrono::steady_clock::time_point began) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
      .count();
}

[[nodiscard]] std::string ElementError(const MergeError &error) {
  return "semantic OSM merge failed at source element " + std::to_string(error.Id) + " with code " +
         std::to_string(static_cast<int>(error.Code));
}

std::expected<ElementSet, MergeError> FinishChunks(std::vector<ElementSet> &chunks) {
  if (chunks.size() != 1) { return ElementSet::Merge(chunks, kMaxInputElements); }
  const auto &single = chunks.front();
  size_t remaining = kMaxInputElements;
  for (const auto [count, kind] : {std::pair{single.Nodes().size(), ElementKind::Node},
                                   std::pair{single.Ways().size(), ElementKind::Way},
                                   std::pair{single.Relations().size(), ElementKind::Relation}}) {
    if (count > remaining) {
      return std::unexpected(
          MergeError{.Code = MergeErrorCode::BudgetExceeded, .Kind = kind, .Id = 0});
    }
    remaining -= count;
  }
  return std::move(chunks.front());
}

}

std::expected<SourceSnapshot, std::string>
ChunkSetLoader::LoadRegion(std::span<const Data::SourceProvider> providers,
                           std::string_view shippedRoot,
                           const std::stop_token &stop) {
  auto read = ReadRegion(providers, shippedRoot, stop);
  if (!read) { return std::unexpected(std::move(read.error())); }
  return ParseRegion(*read, stop);
}

std::expected<std::vector<SourceChunk>, std::string>
ChunkSetLoader::ReadRegion(std::span<const Data::SourceProvider> providers,
                           std::string_view shippedRoot,
                           const std::stop_token &stop,
                           const RemoteRead &remoteRead) {
  std::vector<SourceChunk> chunks;
  chunks.reserve(providers.size());
  size_t bytes = 0;
  for (const auto &provider : providers) {
    if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
    SourceChunk chunk;
    if (remoteRead || !provider.Endpoint.empty()) {
      if (!remoteRead) { return std::unexpected("official original OSM needs a source transport"); }
      auto fetched = remoteRead(provider, stop);
      if (!fetched) { return std::unexpected(std::move(fetched.error())); }
      chunk = std::move(*fetched);
    } else {
      const std::filesystem::path location(provider.Location);
      const auto path =
          location.is_absolute() ? location : std::filesystem::path(shippedRoot) / location;
      const auto began = std::chrono::steady_clock::now();
      auto xml = ReadTextFile(path.string(), kMaximumXmlBytes);
      if (!xml) { return std::unexpected(std::move(xml.error())); }
      chunk = {.Provider = provider,
               .Xml = std::move(*xml),
               .Origin = provider.Location,
               .ReadMs = MillisecondsSince(began),
               .FromStore = false};
    }
    if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
    if (chunk.Xml.size() > kMaximumXmlBytes || chunk.Xml.size() > kMaxTotalBytes - bytes) {
      return std::unexpected("semantic OSM source exceeds the byte budget");
    }
    bytes += chunk.Xml.size();
    chunks.push_back(std::move(chunk));
  }
  return chunks;
}

std::expected<SourceSnapshot, std::string>
ChunkSetLoader::ParseRegion(std::span<const SourceChunk> input, const std::stop_token &stop) {
  if (std::ranges::any_of(input, [](const auto &chunk) { return chunk.Cell.has_value(); })) {
    return std::unexpected("geographic OSM cells require independent parsing");
  }
  return ParseChunks(input, stop);
}

std::expected<SourceSnapshot, std::string> ChunkSetLoader::ParseCell(const SourceChunk &input,
                                                                     const std::stop_token &stop) {
  const auto bounds = input.Cell ? input.Cell->Bounds() : std::nullopt;
  if (!bounds || bounds != input.Provider.Coverage) {
    return std::unexpected("original OSM cell address does not match its coverage");
  }
  return ParseChunks(std::span(&input, 1), stop);
}

std::expected<SourceSnapshot, std::string>
ChunkSetLoader::ParseChunks(std::span<const SourceChunk> input, const std::stop_token &stop) {
  std::vector<ElementSet> chunks;
  std::vector<Data::SourceCoverage> coverage;
  std::vector<ChunkProvenance> provenance;
  chunks.reserve(input.size());
  coverage.reserve(input.size());
  provenance.reserve(input.size());
  size_t bytes = 0;
  double readMs = 0.0;
  double parseMs = 0.0;
  for (const SourceChunk &source : input) {
    const auto &provider = source.Provider;
    if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
    const auto parseAt = std::chrono::steady_clock::now();
    const std::string_view xml(source.Xml);
    readMs += source.ReadMs;
    if (xml.size() > kMaximumXmlBytes || xml.size() > kMaxTotalBytes - bytes) {
      return std::unexpected("semantic OSM source exceeds the total byte budget");
    }
    bytes += xml.size();
    const std::string_view digest = PayloadDigest(provider);
    const std::string actualDigest = Sha256Hex(xml);
    if (!digest.empty() && actualDigest != digest) {
      return std::unexpected("semantic OSM source '" + provider.Location +
                             "' does not match its sha256 pin");
    }
    auto parsed =
        XmlReader::Read(xml, {.DatasetId = provider.Dataset, .Revision = provider.Revision});
    parseMs += MillisecondsSince(parseAt);
    if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
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
    provenance.push_back({.Location = source.Origin,
                          .PayloadSha256 = actualDigest,
                          .PinVerified = !digest.empty(),
                          .FromStore = source.FromStore});
    chunks.push_back(std::move(*parsed));
  }

  if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
  const auto mergeAt = std::chrono::steady_clock::now();
  auto merged = FinishChunks(chunks);
  parseMs += MillisecondsSince(mergeAt);
  if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
  if (!merged) { return std::unexpected(ElementError(merged.error())); }
  return SourceSnapshot{.Elements = std::move(*merged),
                        .Coverage = std::move(coverage),
                        .SourceBytes = bytes,
                        .ReadMs = readMs,
                        .ParseMs = parseMs,
                        .Chunks = std::move(provenance),
                        .Cell = input.size() == 1 ? input.front().Cell : std::nullopt};
}

std::expected<SourceSnapshot, std::string>
ChunkSetLoader::Load(std::span<const Data::SourceProvider> providers,
                     std::string_view shippedRoot,
                     const std::stop_token &stop) {
  auto loaded = LoadRegion(providers, shippedRoot, stop);
  if (!loaded) { return loaded; }
  if (const auto missing = loaded->Elements.FirstMissingReference()) {
    return std::unexpected("semantic OSM source element " + std::to_string(missing->OwnerId) +
                           " refers to missing element " + std::to_string(missing->MissingId));
  }
  return loaded;
}

}

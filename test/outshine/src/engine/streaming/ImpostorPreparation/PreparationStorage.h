#ifndef OUTSHINE_TEST_PREPARATIONSTORAGE_H
#define OUTSHINE_TEST_PREPARATIONSTORAGE_H
#include "PreparationCommon.h"
#include "PreparedImpostorAssets.h"
#include "ContentStore.h"
#include "Digest.h"
#include "Sha256.h"
#include "Tasks.h"
#include <algorithm>
#include <iterator>
#include <latch>
#include <optional>
#include <print>
#include <thread>
#include <utility>

namespace outshine::Test::CrownFixture {
inline bool SameView(const Content::ImpostorAtlas::View &before,
                     const Content::ImpostorAtlas::View &after) {
  if (before.TowardEye != after.TowardEye || before.Texels.size() != after.Texels.size()) {
    return false;
  }
  for (size_t at = 0; at < before.Texels.size(); ++at) {
    const auto &a = before.Texels[at];
    const auto &b = after.Texels[at];
    if (a.Normal != b.Normal || a.Depth != b.Depth || a.Surface != b.Surface) { return false; }
  }
  return true;
}

inline void CheckSameAtlas(const Content::ImpostorAtlas &atlas,
                           const Content::ImpostorAtlas &restored) {
  bool identical = restored.Pixels() == atlas.Pixels() && restored.CentreM() == atlas.CentreM() &&
                   restored.HalfExtentM() == atlas.HalfExtentM() &&
                   restored.Surfaces() == atlas.Surfaces() &&
                   restored.Views().size() == atlas.Views().size();
  if (identical) {
    for (size_t view = 0; view < atlas.Views().size(); ++view) {
      identical &= SameView(atlas.Views()[view], restored.Views()[view]);
    }
  }
  CHECK(identical,
        "every material, normal, depth, identity and camera bound survives storage exactly");
}

inline void Resign(std::vector<uint8_t> &bytes) {
  uint64_t hash = kDigestBasis;
  for (size_t at = 0; at < bytes.size() - kChecksumBytes; ++at) {
    hash = DigestFolded(hash, bytes[at]);
  }
  for (size_t at = 0; at < kChecksumBytes; ++at) {
    bytes[bytes.size() - kChecksumBytes + at] = static_cast<uint8_t>(hash >> (at * kChecksumBytes));
  }
}

inline void CheckCorruption(const Content::ImpostorAtlas &atlas,
                            const std::vector<uint8_t> &encoded,
                            const std::string &provenance) {
  std::string error;
  CHECK(!Content::ImpostorAtlas::Decode(encoded, provenance + "changed", error),
        "changed generator or species provenance invalidates the cache");
  CHECK(!atlas.Encode("", error), "unidentified cache data is refused");
  for (const size_t length : {size_t{0}, kChecksumBytes, kShortHeaderBytes, encoded.size() - 1}) {
    CHECK(!Content::ImpostorAtlas::Decode(std::span(encoded).first(length), provenance, error),
          "truncated artifact data is refused");
  }
  auto corrupt = encoded;
  corrupt[kCorruptByte] ^= 1;
  CHECK(!Content::ImpostorAtlas::Decode(corrupt, provenance, error),
        "payload corruption is detected");
  for (const size_t field : kCorruptFields) {
    corrupt = encoded;
    for (size_t at = 0; at < sizeof(uint32_t); ++at) { corrupt[field + at] = kOpaque; }
    Resign(corrupt);
    CHECK(!Content::ImpostorAtlas::Decode(corrupt, provenance, error),
          "unsupported version and oversized dimensions fail even with a valid checksum");
  }
  corrupt = encoded;
  std::ranges::copy(kNanBytes, corrupt.begin() + kNanOffset);
  Resign(corrupt);
  CHECK(!Content::ImpostorAtlas::Decode(corrupt, provenance, error),
        "nonfinite material data is refused even with a valid checksum");
}

inline std::optional<std::vector<uint8_t>> CheckStorage(const Content::ImpostorAtlas &atlas,
                                                        const TreeSpecies &species,
                                                        const std::string &text) {
  std::string error;
  const std::string provenance =
      ImpostorAtlasProvenance(species.Definition(), {.Pixels = kPixels, .Views = kViews});
  CHECK(provenance !=
                ImpostorAtlasProvenance(text + "changed", {.Pixels = kPixels, .Views = kViews}) &&
            provenance != ImpostorAtlasProvenance(text, {.Pixels = 2 * kPixels, .Views = kViews}) &&
            provenance != ImpostorAtlasProvenance(text, {.Pixels = kPixels, .Views = 2 * kViews}),
        "species and capture shape participate in the compiled producer identity");
  auto encoded = atlas.Encode(provenance, error);
  CHECK(encoded.has_value(), "real crown data encodes without material loss");
  if (!encoded) {
    std::println("{}", error);
    return std::nullopt;
  }
  const auto restored = Content::ImpostorAtlas::Decode(*encoded, provenance, error);
  CHECK(restored.has_value(), "versioned crown data reads back");
  if (!restored) { return std::nullopt; }
  CheckSameAtlas(atlas, *restored);
  CHECK(restored->Encode(provenance, error) == encoded,
        "the artifact has a canonical byte-for-byte round trip");
  CheckCorruption(atlas, *encoded, provenance);
  WriteBytes(*encoded, OutputDirectory() + "/birch.crown");
  std::println("crown artifact {} bytes; source/raw-data equality checked; compiled producer, "
               "species and capture shape identified",
               encoded->size());
  return encoded;
}

inline void CheckPublicationCollision(Data::ContentStore &raw,
                                      const Data::ContentStore::Config &config,
                                      const std::vector<uint8_t> &bytes,
                                      const std::string &provenance) {
  const auto key = Sha256Hex(provenance);
  const auto occupied = config.Directory + "/." + key + ".0";
  {
    std::ofstream held(occupied);
    held << "occupied";
  }
  CHECK(raw.Keep(key, bytes.data(), bytes.size()),
        "source bytes publish despite an occupied temporary name");
  std::ifstream held(occupied);
  const std::string retained{std::istreambuf_iterator<char>(held), {}};
  CHECK(retained == "occupied",
        "exclusive publication never truncates another writer's temporary file");
}

inline void CheckRawStore(Data::ContentStore &raw,
                          const Data::ContentStore::Config &config,
                          const std::vector<uint8_t> &bytes,
                          const std::string &provenance) {
  const auto key = Sha256Hex(provenance);
  CHECK(raw.Keep(key, bytes.data(), bytes.size()),
        "a second store can publish the same key independently");
  CHECK(!raw.Read(key, bytes.size() - 1),
        "the byte budget rejects a large file before returning its payload");
  const auto read = raw.Read(key, bytes.size());
  CHECK(read.has_value() && *read == bytes,
        "the complete payload is readable at its exact byte budget");
  std::filesystem::create_directory(config.Directory + "/blocked");
  CHECK(!raw.Keep("blocked", bytes.data(), bytes.size()) &&
            std::filesystem::is_directory(config.Directory + "/blocked"),
        "failed publication preserves the existing destination");
  CHECK(raw.Keep(Sha256Hex(provenance + "changed"), bytes.data(), bytes.size()),
        "the stale-cache control contains real bytes under a different provenance key");
}

inline bool PublishStale(const std::string &directory,
                         const std::vector<uint8_t> &bytes,
                         const std::string &provenance) {
  auto native = AssetCache::Open(directory + "/prototypes.sqlite");
  CHECK(native.has_value(), "native prototype index can be opened independently");
  if (!native) { return false; }
  const AssetRecord stale{.Key = Sha256Hex(provenance + "changed"),
                          .Kind = "impostor-prototype",
                          .Bounds = {.Min = {{-kBoundsM, -kBoundsM, -kBoundsM}},
                                     .Max = {{kBoundsM, kBoundsM, kBoundsM}}},
                          .Package = {},
                          .ByteCount = bytes.size(),
                          .Parent = {}};
  CHECK((*native)->Publish(std::span(&stale, size_t{1}), bytes).has_value(),
        "native stale control has valid indexed bytes but the wrong generator provenance");
  return true;
}

inline void CheckPendingReads(Content::PreparedImpostorAssets &cache,
                              Tasks &worker,
                              const std::string &provenance) {
  std::latch entered(1);
  std::latch release(1);
  const auto blocker = worker.Post([&] {
    entered.count_down();
    release.wait();
  });
  entered.wait();
  CHECK(cache.Read(provenance) == Content::PreparedImpostorAssets::Request::Queued,
        "a cache read posts while its worker is blocked");
  CHECK(cache.Read(provenance) == Content::PreparedImpostorAssets::Request::Existing,
        "duplicate pending keys share one request");
  CHECK(cache.Read(provenance + "changed") == Content::PreparedImpostorAssets::Request::Queued,
        "the second pending slot is available");
  CHECK(cache.Read(provenance + "third") == Content::PreparedImpostorAssets::Request::Full,
        "the pending budget rejects excess requests");
  CHECK(!cache.Take(), "polling returns without waiting for blocked IO");
  release.count_down();
  worker.Wait(blocker);
}

inline void CheckLoaded(Content::PreparedImpostorAssets &cache,
                        std::optional<Content::ImpostorAtlas> &atlas,
                        const std::vector<uint8_t> &bytes,
                        const std::string &provenance) {
  std::vector<Content::PreparedImpostorAssets::Loaded> loaded;
  const auto began = std::chrono::steady_clock::now();
  while (loaded.size() < 2 && std::chrono::steady_clock::now() - began < kLoadTimeout) {
    if (auto ready = cache.Take()) {
      loaded.push_back(std::move(*ready));
    } else {
      std::this_thread::sleep_for(kPollInterval);
    }
  }
  CHECK(loaded.size() == 2, "bounded asynchronous reads complete");
  CHECK(cache.Costs().Hits == 1 && cache.Costs().Misses == 1 &&
            cache.Costs().ReadBytes == 2 * bytes.size(),
        "native IO reads both complete packages and rejects the stale decoded product");
  if (loaded.size() != 2) { return; }
  std::string error;
  const auto &ready = loaded[0].Atlas;
  bool exact = false;
  if (ready) {
    const auto encoded = ready->Encode(provenance, error);
    exact = encoded.has_value() && *encoded == bytes;
  }
  CHECK(exact, "the worker returns the exact published crown artifact");
  CHECK(!loaded[1].Atlas && !loaded[1].Error.empty(),
        "a stale on-disk artifact is rejected after IO");
  if (loaded[0].Atlas) { atlas = std::move(loaded[0].Atlas); }
}

inline bool CheckCache(std::optional<Content::ImpostorAtlas> &atlas,
                       const std::vector<uint8_t> &bytes,
                       const std::string &provenance,
                       Tasks &worker) {
  if (!atlas) { return false; }
  const std::string directory = OutputDirectory() + "/cache";
  std::filesystem::remove_all(directory);
  const Data::ContentStore::Config config{.Directory = directory + "/sources"};
  Data::ContentStore raw(config);
  Content::PreparedImpostorAssets cache(worker, {.Directory = directory});
  CheckPublicationCollision(raw, config, bytes, provenance);
  std::string error;
  CHECK(cache.Publish(*atlas, provenance, error),
        "native prototype publishes through the common package and spatial-index service");
  CheckRawStore(raw, config, bytes, provenance);
  if (!PublishStale(directory, bytes, provenance)) { return false; }
  CheckPendingReads(cache, worker, provenance);
  CheckLoaded(cache, atlas, bytes, provenance);
  {
    Content::PreparedImpostorAssets draining(worker, {.Directory = directory});
    CHECK(draining.Read(provenance) == Content::PreparedImpostorAssets::Request::Queued,
          "a pending read can be safely drained during cache destruction");
  }
  return true;
}
}
#endif

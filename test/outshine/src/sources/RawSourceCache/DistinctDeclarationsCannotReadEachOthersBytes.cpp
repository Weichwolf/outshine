#include "Check.h"
#include "RawSourceCache.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;

void CheckFreeFields(const std::filesystem::path &root) {
  Data::SourceDecl first{};
  first.Id = "a";
  first.Revision = "b\n1\nc";
  Data::SourceDecl second{};
  second.Id = "a\n1\nb";
  second.Revision = "c";
  const auto at = Data::Address::Whole(0);
  const auto keyA = Data::ContentKey(first, at);
  const auto keyB = Data::ContentKey(second, at);
  CHECK(keyA.size() == 64 && keyB.size() == 64 && keyA != keyB,
        "free ID and revision fields cannot alias raw content");
  CHECK(Data::SourceKey(first) != Data::SourceKey(second),
        "free ID and revision fields cannot alias source metadata");
  CHECK(keyA == Data::ContentKey(first, at) && Data::SourceKey(first) == Data::SourceKey(first),
        "unchanged declarations keep stable identities");
  constexpr auto legacy = "e87743fb489e44922b9e9fb242c843ef49ea001c827f5a1feff236192da4c572";
  CHECK(keyA != legacy && keyB != legacy,
        "neither declaration inherits the old ambiguous namespace");
  const std::array<uint8_t, 3> bytes{17, 25, 81};
  {
    Data::RawSourceCache cache({.Directory = root.string()});
    CHECK(cache.Keep(legacy, bytes.data(), bytes.size()), "the original raw entry is retained");
    CHECK(cache.Lookup(keyA).Where == Data::RawSourceCache::Presence::Unknown &&
              cache.Lookup(keyB).Where == Data::RawSourceCache::Presence::Unknown,
          "old unframed bytes cannot silently satisfy a new declaration");
    CHECK(cache.Keep(keyA, bytes.data(), bytes.size()), "the first canonical package is stored");
  }
  Data::RawSourceCache fresh({.Directory = root.string()});
  const auto own = fresh.Lookup(keyA);
  const auto foreign = fresh.Lookup(keyB);
  CHECK(own.Where == Data::RawSourceCache::Presence::Bytes &&
            own.Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end()),
        "a fresh owner restores the exact bytes for the same declaration");
  CHECK(foreign.Where == Data::RawSourceCache::Presence::Unknown,
        "a different declaration cannot read the stored bytes");
  CHECK(fresh.Lookup(legacy).Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end()),
        "the version transition preserves old data without treating it as a safe new hit");
  std::cout << "SOURCE_DECLARATIONS keys_equal=" << (keyA == keyB)
            << " foreign_hit=" << (foreign.Where == Data::RawSourceCache::Presence::Bytes) << '\n';
}

void CheckEndpointPayloadAndRanges() {
  Data::SourceDecl first{};
  first.Id = "fixture";
  first.Endpoint = std::string(64, 'a');
  auto second = first;
  second.Endpoint.clear();
  second.PayloadSha256 = first.Endpoint;
  const auto at = Data::Address::Whole(0);
  CHECK(Data::ContentKey(first, at) != Data::ContentKey(second, at),
        "empty endpoint and payload fields cannot exchange their meanings");
  const Data::ByteRange range{.First = 7, .Length = 13};
  const auto key = Data::ContentKey(first, at, range, "v1");
  CHECK(key == Data::ContentKey(first, at, range, "v1") &&
            key != Data::ContentKey(first, at, range, "v2") &&
            key != Data::ContentKey(first, at, Data::ByteRange{.First = 8, .Length = 13}, "v1") &&
            key != Data::ContentKey(first, at, Data::ByteRange{.First = 7, .Length = 14}, "v1") &&
            key != Data::ContentKey(second, at, range, "v1"),
        "object identity, exact interval and entity tag all bind a range");
  CHECK(Data::ContentKey(first, at, std::nullopt, "unused") == Data::ContentKey(first, at),
        "an entity tag without a range keeps the existing whole-object contract");
  auto wire = first;
  wire.Wire = Data::WireFormat::TerrariumWebp;
  CHECK(Data::SourceKey(wire) != Data::SourceKey(first) &&
            Data::ContentKey(wire, at) == Data::ContentKey(first, at),
        "decode format changes source identity while preserving the same raw object's field set");
}
}

int main() {
  using namespace outshine::Test;
  const auto root = std::filesystem::path(PlantedPath("source-key-framing"));
  std::error_code error;
  std::filesystem::create_directories(root, error);
  CHECK(!error, "the isolated source cache directory is available");
  CheckFreeFields(root);
  CheckEndpointPayloadAndRanges();
  std::filesystem::remove_all(root, error);
  CHECK(!error, "all test-owned raw cache files are removed after closing their owners");
  return Report();
}

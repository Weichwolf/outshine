#include "Check.h"
#include "StarCatalogueSource.h"
#include "OfflineSourceTransport.h"
#include "RawSourceCache.h"
#include "ReadTextFile.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Test;
using Generators::Sky::StarCatalogueSource;
constexpr size_t kReferenceBytesMost = size_t{1024} * 1024u;

std::optional<SourceResponse::Settled> Read(const StarCatalogueSource &source, Address at) {
  OfflineSourceTransport transport;
  CHECK(source.Begin(at, transport) == FetchStart(Ticket::None),
        "local catalogue requests start without a network operation");
  auto reply = source.Collect(at, Ticket::None, transport);
  return reply.Take();
}

void CheckBands() {
  const std::filesystem::path directory = "data/sky/stars";
  const StarCatalogueSource source(
      directory.string(), "fixture-version", Rank{0}, AbsencePolicy::Fail);
  const auto &decl = source.Declaration();
  CHECK(decl.Id == "hyg.bands" && decl.Version == 1 && decl.Revision == "fixture-version" &&
            decl.Kind == DataKind::StarCatalogue && decl.Wire == WireFormat::StarBandBinary &&
            decl.How == Scheme::WholeWorld && decl.Keeps == Cacheability::Never,
        "catalogue identity, encoding and acquisition policy remain stable");
  CHECK(decl.MaximumPayloadBytes > 0, "the local source declares a finite allocation bound");
  CHECK(source.Covers(SourceRequest(DataKind::Elevation, Address::Whole(0))) == Coverage::Outside &&
            source.Covers(
                SourceRequest(DataKind::StarCatalogue, Address::At({.Zoom = 0, .X = 0, .Y = 0}))) ==
                Coverage::Outside,
        "other data kinds and geographic tiles are not catalogue bands");
  auto previous = decl;
  previous.MaximumPayloadBytes = 0;
  CHECK(SourceKey(previous) == SourceKey(decl),
        "an IO allocation limit does not change source identity");
  for (uint32_t band = 0; band < StarCatalogueSource::kBands; ++band) {
    const auto at = Address::Whole(band);
    CHECK(source.Covers(SourceRequest(DataKind::StarCatalogue, at)) == Coverage::Inside &&
              source.Serves(SourceRequest(DataKind::StarCatalogue, at)) == at,
          "each shipped band retains its original request address");
    CHECK(ContentKey(previous, at) == ContentKey(decl, at),
          "the raw-content identity excludes a runtime memory allowance");
    const auto expected =
        ReadTextFile((directory / std::format("band{}.bin", band)).string(), kReferenceBytesMost);
    const auto reply = Read(source, at);
    CHECK(expected && reply && reply->What == Meaning::Bytes &&
              std::ranges::equal(
                  reply->Bytes,
                  *expected,
                  [](uint8_t one, char two) { return one == static_cast<uint8_t>(two); }),
          "all four embedded binary bands are delivered byte for byte");
  }
}

void Write(const std::filesystem::path &file) {
  const std::array<char, 5> bytes{'s', '\0', static_cast<char>(-1), 'x', '\n'};
  std::ofstream output(file, std::ios::binary);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  CHECK(output.good(), "the binary source fixture is written completely");
}

void CheckLongPath(const std::filesystem::path &root) {
  auto directory = root;
  constexpr size_t kComponentBytes = 90;
  for (int part = 0; part < 6; ++part) { directory /= std::string(kComponentBytes, 'a'); }
  std::error_code error;
  CHECK(std::filesystem::create_directories(directory, error) && !error,
        "a legal source path longer than the old stack buffer exists");
  Write(directory / "band0.bin");
  const StarCatalogueSource source(
      directory.string(), "fixture-version", Rank{0}, AbsencePolicy::Fail);
  const auto reply = Read(source, Address::Whole(0));
  CHECK(reply && reply->What == Meaning::Bytes && reply->Bytes.size() == 5 &&
            reply->Bytes[0] == 's' && reply->Bytes[1] == 0 && reply->Bytes[2] == UINT8_MAX &&
            reply->Bytes[3] == 'x' && reply->Bytes[4] == '\n',
        "legal long paths are not truncated and binary values remain exact");
}

void CheckFailures(const std::filesystem::path &root) {
  const StarCatalogueSource source(root.string(), "fixture-version", Rank{0}, AbsencePolicy::Fail);
  auto missing = Read(source, Address::Whole(0));
  CHECK(missing && missing->What == Meaning::Refused && missing->Bytes.empty(),
        "a missing required catalogue does not become successful empty data");
  Write(root / "band4.bin");
  const auto outside = Read(source, Address::Whole(StarCatalogueSource::kBands));
  CHECK(outside && outside->What == Meaning::Refused &&
            outside->Reason == SourceFailureReason::InvalidRequest && outside->Bytes.empty(),
        "an undeclared band cannot be read even when a matching filename exists");
  const auto file = root / "band0.bin";
  Write(file);
  const StarCatalogueSource invalidPath(
      file.string() + std::string("\0ignored", 8), "fixture-version", Rank{0}, AbsencePolicy::Fail);
  const auto invalid = Read(invalidPath, Address::Whole(0));
  CHECK(invalid && invalid->What == Meaning::Refused &&
            invalid->Reason == SourceFailureReason::InvalidRequest && invalid->Bytes.empty(),
        "embedded NUL cannot redirect collection to another file");
  std::error_code error;
  std::filesystem::resize_file(file, 0, error);
  CHECK(!error, "the empty catalogue fixture exists");
  const auto empty = Read(source, Address::Whole(0));
  CHECK(empty && empty->What == Meaning::Refused && empty->Bytes.empty(),
        "an empty required band is a failed source, not a complete catalogue");
  const size_t limit = std::max(kReferenceBytesMost, source.Declaration().MaximumPayloadBytes);
  std::filesystem::resize_file(file, limit + 1, error);
  CHECK(!error, "an oversized sparse catalogue fixture exists");
  const auto excessive = Read(source, Address::Whole(0));
  CHECK(excessive && excessive->What == Meaning::Refused &&
            excessive->Reason == SourceFailureReason::CapacityRefused && excessive->Bytes.empty(),
        "oversized files are refused without returning a data product");
}
}

int main() {
  CheckBands();
  const auto root = std::filesystem::temp_directory_path() /
                    std::format("outshine-star-source-{}",
                                std::chrono::steady_clock::now().time_since_epoch().count());
  std::error_code error;
  const bool created = std::filesystem::create_directory(root, error);
  CHECK(created && !error, "isolated source fixtures exist");
  if (!created || error) { return Report(); }
  CheckLongPath(root);
  CheckFailures(root);
  std::filesystem::remove_all(root, error);
  CHECK(!error, "source fixtures are removed");
  return Report();
}

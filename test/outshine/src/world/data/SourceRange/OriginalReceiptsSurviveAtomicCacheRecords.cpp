#include "Check.h"
#include "ContentStore.h"
#include "Sha256.h"
#include "SourceRange.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
std::vector<uint8_t> Record(std::string_view header) {
  std::vector<uint8_t> record(header.begin(), header.end());
  record.insert(record.end(), {4, 5, 6});
  const auto seal = outshine::Sha256Hex(record.data(), record.size());
  record.insert(record.end(), seal.begin(), seal.end());
  return record;
}
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  const RangeResponse origin{
      .Bytes = {.First = 100, .Length = 3}, .TotalBytes = 1000, .EntityTag = "\"rev-1\""};
  constexpr std::array<uint8_t, 3> bytes{4, 5, 6};
  constexpr std::string_view header = "range/1\n100\n3\n1000\n\"rev-1\"\n";
  constexpr std::string_view digest =
      "07d3c8f29de16fd3660657dad19acbd3bd33226f6bc9c572d4d326ae2df8e9b0";
  std::vector<uint8_t> golden(header.begin(), header.end());
  golden.insert(golden.end(), bytes.begin(), bytes.end());
  golden.insert(golden.end(), digest.begin(), digest.end());
  const auto packed = PackSourceRange(bytes, origin);
  CHECK(packed && *packed == golden,
        "cache record agrees with independently sealed original bytes");
  if (!packed) { return Report(); }
  const auto decoded = UnpackSourceRange(golden);
  CHECK(decoded && decoded->Bytes == origin.Bytes && decoded->TotalBytes == origin.TotalBytes &&
            decoded->EntityTag == origin.EntityTag && golden == std::vector<uint8_t>({4, 5, 6}),
        "cache restores receipt and exact original bytes without raster reconstruction");
  for (std::string_view bad : {"range/2\n100\n3\n1000\n\"rev-1\"\n",
                               "range/1\n-1\n3\n1000\n\"rev-1\"\n",
                               "range/1\n18446744073709551616\n3\n1000\n\"rev-1\"\n",
                               "range/1\n100\n0\n1000\n\"rev-1\"\n",
                               "range/1\n100\n2\n1000\n\"rev-1\"\n",
                               "range/1\n100\n3\n102\n\"rev-1\"\n",
                               "range/1\n100\n3\n1000\nW/\"rev-1\"\n"}) {
    auto malformed = Record(bad);
    const auto before = malformed;
    CHECK(!UnpackSourceRange(malformed) && malformed == before,
          "valid seals cannot admit invalid metadata; failed decode preserves the input");
  }
  for (size_t changed : {size_t{0}, header.size(), packed->size() - 1}) {
    auto corrupted = *packed;
    corrupted[changed] ^= 1;
    CHECK(!UnpackSourceRange(corrupted),
          "metadata, body and checksum corruption cannot retain a receipt");
  }
  CHECK(!PackSourceRange(std::span<const uint8_t>(bytes).first(2), origin),
        "encoding refuses an origin whose byte length disagrees with the received payload");
  SourceDecl decl;
  decl.Id = "native-raw-range";
  decl.Endpoint = "https://original.example/object.tif";
  const auto at = Address::AtCell({54, 9});
  const auto key = ContentKey(decl, at, origin.Bytes, origin.EntityTag);
  CHECK(key != ContentKey(decl, at) && key != ContentKey(decl, at, origin.Bytes, "\"rev-2\"") &&
            key != ContentKey(decl, at, ByteRange{.First = 101, .Length = 3}, origin.EntityTag) &&
            key != ContentKey(decl, Address::AtCell({54, 10}), origin.Bytes, origin.EntityTag),
        "object, cell, interval and revision each contribute to cache identity");
  auto directory = (std::filesystem::temp_directory_path() / "outshine-ranges-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated raw cache");
  const ContentStore::Config config{.Directory = directory};
  {
    ContentStore store(config);
    CHECK(store.Keep(key, packed->data(), packed->size()),
          "receipt and raw body written atomically");
  }
  ContentStore reopened(config);
  auto stored = reopened.Read(key, bytes.size() + MaximumRangeRecordOverhead);
  CHECK(stored && UnpackSourceRange(*stored) && *stored == std::vector<uint8_t>({4, 5, 6}),
        "reopened existing content store delivers unchanged original bytes and a usable receipt");
  CHECK(!reopened.Read(ContentKey(decl, at)),
        "partial cache record cannot be mistaken for a whole object");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "fixture removed");
  return Report();
}

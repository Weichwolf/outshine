#include "Address.h"
#include "ByteArchive.h"
#include "Check.h"
#include "OfflineSourceTransport.h"
#include "PmTilesDirectory.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "TerrariumSource.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Data;
using Generators::Terrain::PmTilesDirectory;
using Generators::Terrain::TerrariumSource;

constexpr TileId kMissing{.Zoom = 17, .X = 69219, .Y = 45964};
constexpr TileId kPresent{.Zoom = 17, .X = 69218, .Y = 45965};
constexpr TileId kParent{.Zoom = 16, .X = 34609, .Y = 22982};
constexpr ByteRange kPrefix{.First = 0, .Length = PmTilesDirectory::RootWindowBytes};
constexpr auto kEntityTag = "\"archive-v1\"";
constexpr uint64_t kHeaderBytes = 127;
constexpr uint64_t kArchiveBytes = 32 * 1024;

// Generated with pmtiles.serialize_directory using kParent and kPresent.
constexpr std::array<uint8_t, 29> kRoot{31, 139, 8,  0,   0,   0,  0,  0, 0,  255,
                                        99, 156, 52, 237, 228, 55, 33, 6, 53, 70,
                                        0,  64,  54, 149, 175, 9,  0,  0, 0};
constexpr std::array<uint8_t, 38> kLeaf{
    31,  139, 8,   0,  0,   0,   0,  0,  0,   255, 99, 154, 52, 237, 228, 55, 161, 93, 135,
    110, 63,  182, 96, 100, 100, 98, 98, 100, 0,   0,  114, 7,  55,  185, 17, 0,   0,  0};
constexpr ByteRange kLeafRange{.First = kPrefix.Length, .Length = kLeaf.size()};

std::vector<uint8_t> MakePrefix() {
  constexpr std::array<uint8_t, 8> magic{'P', 'M', 'T', 'i', 'l', 'e', 's', 3};
  ByteWriter writer(kPrefix.Length);
  const uint64_t dataOffset = kLeafRange.First + kLeafRange.Length;
  if (!writer.Put(magic) || !writer.Number(kHeaderBytes) ||
      !writer.Number(uint64_t{kRoot.size()}) || !writer.Number(dataOffset) ||
      !writer.Number(uint64_t{0}) || !writer.Number(kLeafRange.First) ||
      !writer.Number(kLeafRange.Length) || !writer.Number(dataOffset) ||
      !writer.Number(kArchiveBytes - dataOffset)) {
    return {};
  }
  std::vector<uint8_t> data(kPrefix.Length);
  std::ranges::copy(writer.Bytes(), data.begin());
  constexpr auto gzipOffset = 97, compressionOffset = 98, typeOffset = 99;
  constexpr auto minimumZoomOffset = 100, maximumZoomOffset = 101;
  data[gzipOffset] = 2;
  data[compressionOffset] = 1;
  data[typeOffset] = 4;
  data[minimumZoomOffset] = kParent.Zoom;
  data[maximumZoomOffset] = kPresent.Zoom;
  std::ranges::copy(kRoot, data.begin() + kHeaderBytes);
  return data;
}

class DirectoryTransport final : public SourceTransport {
public:
  struct Read {
    std::string Url;
    ByteRange Range;
    std::string EntityTag;
  };

  bool Replaced = false;
  std::vector<Read> Reads;

  FetchStart Begin(const std::string &url) override { return Begin(url, ByteRange{}); }

  FetchStart Begin(const std::string &url, size_t) override { return Begin(url); }

  FetchStart Begin(const std::string &url, ByteRange range, std::string_view tag = {}) override {
    Reads.push_back({.Url = url, .Range = range, .EntityTag = std::string(tag)});
    return static_cast<Ticket>(Reads.size());
  }

  Wire Collect(Ticket ticket) override {
    const auto &read = Reads[static_cast<size_t>(ticket) - 1];
    if (read.Range == kPrefix) {
      return Wire::Answered(
          MakePrefix(), {.Bytes = kPrefix, .TotalBytes = kArchiveBytes, .EntityTag = kEntityTag});
    }
    if (read.Range == kLeafRange) {
      return Wire::Answered(std::vector<uint8_t>(kLeaf.begin(), kLeaf.end()),
                            {.Bytes = kLeafRange,
                             .TotalBytes = kArchiveBytes,
                             .EntityTag = Replaced ? "\"archive-v2\"" : kEntityTag});
    }
    return Wire::Answered(200, {1, 2});
  }

  void Cancel(Ticket) override {}
};

void AddTerrain(SourceSet &sources) {
  auto terrain = std::make_unique<TerrariumSource>(
      SourceDecl{.Id = "mapterhorn.terrarium",
                 .Revision = {},
                 .Endpoint = std::string(TerrariumSource::MapterhornEndpoint),
                 .Wire = WireFormat::TerrariumWebp,
                 .OnAbsent = AbsencePolicy::Fail,
                 .MaxZoom = kPresent.Zoom,
                 .AncestorFill = true,
                 .MaximumPayloadBytes = 1024,
                 .PayloadSha256 = {},
                 .Schema = {},
                 .TileAbsence = TileAbsencePolicy::Parent});
  CHECK(sources.Add(std::move(terrain)) == SourceSet::Registration::Accepted,
        "a Mapterhorn web tile source registers");
}

std::optional<SourceDelivery::Answer>
ReadTile(SourceSet &sources, SourceTransport &transport, TileId tile) {
  auto query = sources.Ask(SourceRequest(DataKind::Elevation, Address::At(tile)));
  for (int poll = 0; poll < 8; ++poll) {
    auto delivered = sources.Collect(query, transport);
    if (delivered.Where() != SourceDelivery::State::Pending) { return delivered.Take(); }
  }
  SourceSet::Abandon(query, transport);
  return std::nullopt;
}

void CheckColdAndWarm(const std::string &path) {
  {
    RawSourceCache store({.Directory = path});
    SourceSet sources(store);
    AddTerrain(sources);
    DirectoryTransport transport;
    auto missing = sources.Ask(SourceRequest(DataKind::Elevation, Address::At(kMissing)));
    CHECK(sources.Collect(missing, transport).Where() == SourceDelivery::State::Pending,
          "the cold query starts the regional directory");
    const auto existing = ReadTile(sources, transport, kPresent);
    const auto fallback = sources.Collect(missing, transport).Take();
    CHECK(fallback && fallback->At == Address::At(kParent) && existing &&
              existing->At == Address::At(kPresent),
          "shared coverage preserves fine tiles and delivers the true parent for rejected detail");
    if (transport.Reads.size() == 4) {
      CHECK(transport.Reads[0].Url == "https://download.mapterhorn.com/6-33-22.pmtiles" &&
                transport.Reads[0].Range == kPrefix && transport.Reads[0].EntityTag.empty() &&
                transport.Reads[1].Range == kLeafRange &&
                transport.Reads[1].EntityTag == kEntityTag &&
                transport.Reads[2].Url.ends_with("/17/69218/45965.webp") &&
                transport.Reads[3].Url.ends_with("/16/34609/22982.webp"),
            "one pinned directory is shared and only present coordinates get tile requests");
    } else {
      CHECK(false, "exactly two directory requests and two tile requests are expected");
    }
    CHECK(sources.Counters().RemoteStarts == 4, "network telemetry includes the directory IO");
  }
  {
    RawSourceCache store({.Directory = path});
    SourceSet sources(store);
    AddTerrain(sources);
    OfflineSourceTransport transport;
    const auto fallback = ReadTile(sources, transport, kMissing);
    CHECK(fallback && fallback->At == Address::At(kParent) && sources.Counters().RemoteStarts == 0,
          "a reopened cache resolves the same parent with networking unavailable");
  }
}

void CheckChangedArchive(const std::string &path) {
  RawSourceCache store({.Directory = path});
  SourceSet sources(store);
  AddTerrain(sources);
  DirectoryTransport transport;
  transport.Replaced = true;
  const auto received = ReadTile(sources, transport, kMissing);
  CHECK(received && received->At == Address::At(kMissing) && transport.Reads.size() == 3 &&
            transport.Reads.back().Url.ends_with("/17/69219/45964.webp"),
        "a changed archive cannot hide a live tile");
}
}

int main() {
  using namespace outshine::Test;
  auto path = (std::filesystem::temp_directory_path() / "outshine-mapterhorn-XXXXXX").string();
  if (mkdtemp(path.data()) == nullptr) {
    CHECK(false, "an isolated raw cache was available");
    return Report();
  }
  CheckColdAndWarm(path);
  CheckChangedArchive(path + "/changed");
  std::filesystem::remove_all(path);
  return Report();
}

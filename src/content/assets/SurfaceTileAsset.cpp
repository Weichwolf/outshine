#include "SurfaceTileAsset.h"
#include "AssetCache.h"
#include "AssetPublication.h"
#include "Sha256.h"
#include "SurfaceTileStitch.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <array>
#include <deque>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace outshine {
namespace {

constexpr std::string_view kKind = "render-surface-tile-v1";

Data::TileId Parent(Data::TileId tile) {
  return {.Zoom = tile.Zoom - 1, .X = tile.X >> 1, .Y = tile.Y >> 1};
}

std::string Address(Data::TileId tile) {
  return std::format("{}/{}/{}", tile.Zoom, tile.X, tile.Y);
}

std::string Request(std::string_view revision, Data::TileId tile) {
  return Sha256Hex(std::format("{}/{}/{}", kKind, revision, Address(tile)));
}

std::string ParentRequest(std::string_view revision, Data::TileId tile) {
  return tile.Zoom > 0 ? Request(revision, Parent(tile)) : "";
}

bool MatchesAddress(const SurfaceTile &tile, std::string_view revision, const AssetRecord &record) {
  return Request(revision, tile.Coverage) == record.RequestKey &&
         ParentRequest(revision, tile.Coverage) == record.Parent &&
         std::cmp_equal(tile.Coverage.Zoom, record.Level);
}

Box EarthBounds(const SurfaceTile &tile) {
  const auto frame = TangentFrame::At(tile.Origin);
  const Box local{.Min = tile.MinimumM, .Max = tile.MaximumM};
  Box earth;
  for (unsigned corner = 0; corner < 8; ++corner) {
    const auto p = local.Corner(corner);
    earth.Cover(frame.ToEcefPosition({.EastM = p[0], .NorthM = -p[2], .UpM = p[1]}));
  }
  return earth;
}

std::expected<std::optional<SurfaceTile>, std::string> ReadRecord(AssetCache &cache,
                                                                  std::string_view revision,
                                                                  const AssetRecord &record,
                                                                  size_t &requestedBytes) {
  if (record.Kind != kKind || !record.RequestKey ||
      record.ByteCount > SurfaceTile::MaximumPackedBytes) {
    return std::nullopt;
  }
  requestedBytes += static_cast<size_t>(record.ByteCount);
  std::optional<SurfaceTile> tile;
  const auto loaded =
      cache.LoadValidated(record.Key,
                          kKind,
                          SurfaceTile::MaximumPackedBytes,
                          [&](const CachedAsset &asset) -> std::expected<bool, AssetCacheError> {
                            auto decoded = DecodePackedSurfaceTile(asset.Bytes());
                            if (!decoded || !MatchesAddress(*decoded, revision, record)) {
                              return false;
                            }
                            tile = std::move(*decoded);
                            return true;
                          });
  if (!loaded) {
    if (loaded.error() == AssetCacheError::CapacityExceeded) { return std::nullopt; }
    return std::unexpected("surface tile package lookup failed");
  }
  return tile;
}

struct Node {
  const AssetRecord *Record;
  std::vector<size_t> Children;
};

double Clearance(const Box &tile, const Box &eye) {
  Vec3 gap;
  for (size_t axis = 0; axis < 3; ++axis) {
    gap[axis] = std::max({0.0, tile.Min[axis] - eye.Max[axis], eye.Min[axis] - tile.Max[axis]});
  }
  return Length(gap);
}

class CutLoader {
public:
  CutLoader(AssetCache &cache, std::string_view revision, const SurfaceCutDemand &demand)
      : Cache_(cache), Revision_(revision), Demand_(demand) {}

  [[nodiscard]] std::expected<SurfaceCutLookup, std::string>
  Load(std::span<const AssetRecord> records) {
    Nodes_.reserve(records.size());
    for (const auto &record : records) {
      if (!record.RequestKey || !record.Tile) { continue; }
      AddNode(record, *record.RequestKey);
    }
    std::vector<size_t> roots;
    for (size_t at = 0; at < Nodes_.size(); ++at) {
      const auto &record = *Nodes_[at].Record;
      const auto parent = FindParent(record);
      if (!parent) { return std::unexpected(parent.error()); }
      if (!*parent) {
        roots.push_back(at);
      } else if (record.Level > 0 && Nodes_[**parent].Record->Level == record.Level - 1) {
        Nodes_[**parent].Children.push_back(at);
      } else {
        return std::unexpected("surface tile index contains an invalid parent");
      }
    }
    for (const size_t root : roots) {
      const auto loaded = Visit(*Nodes_[root].Record);
      if (!loaded) { return std::unexpected(loaded.error()); }
    }
    return std::move(Cut_);
  }

private:
  size_t AddNode(const AssetRecord &record, std::string_view request) {
    const size_t index = Nodes_.size();
    Addresses_.emplace(request, index);
    Nodes_.push_back({.Record = &record, .Children = {}});
    return index;
  }

  [[nodiscard]] std::expected<std::optional<size_t>, std::string>
  FindParent(const AssetRecord &record) {
    if (record.Parent.empty() || MissingParents_.contains(record.Parent)) { return std::nullopt; }
    const auto known = Addresses_.find(record.Parent);
    if (known != Addresses_.end()) { return known->second; }
    const auto bound = Cache_.FindRequest(record.Parent);
    if (!bound) { return std::unexpected("surface parent request lookup failed"); }
    if (!*bound) {
      MissingParents_.insert(record.Parent);
      return std::nullopt;
    }
    auto tile = Cache_.FindTile((**bound).Key);
    if (!tile || !*tile || !(**tile).Tile || (**tile).Tile->Revision != Revision_ ||
        (**tile).Kind != kKind || record.Level == 0 || (**tile).Level != record.Level - 1) {
      return std::unexpected("surface cut index contains an invalid bound parent");
    }
    (**tile).RequestKey = record.Parent;
    return AddNode(Ancestors_.emplace_back(std::move(**tile)), record.Parent);
  }

  [[nodiscard]] std::expected<bool, std::string>
  VisitPartition(std::span<const AssetRecord *const> children) {
    const size_t checkpoint = Cut_.Products.size();
    const size_t groups = Cut_.Groups.size();
    for (const auto *child : children) {
      const auto loaded = Visit(*child);
      if (!loaded) { return std::unexpected(loaded.error()); }
      if (!*loaded) {
        Cut_.Products.resize(checkpoint);
        Cut_.Groups.resize(groups);
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] std::expected<bool, std::string> VisitGroup(const AssetRecord &record) {
    if (!Demand_.GroupDemand) { return false; }
    auto replacement = LoadGroupImpostor(Cache_, record, *Demand_.GroupDemand);
    if (!replacement) { return std::unexpected(replacement.error()); }
    Cut_.GroupRequestedBytes += replacement->RequestedBytes;
    if (!replacement->Product) { return false; }
    auto &group = *replacement->Product;
    auto ground = std::move(group.Ground);
    if (!ground) {
      auto mesh = ReadRecord(Cache_, Revision_, record, Cut_.RequestedBytes);
      if (!mesh) { return std::unexpected(mesh.error()); }
      if (!*mesh) { return false; }
      ground = std::move(*mesh);
    }
    Cut_.Groups.push_back({.Origin = ground->Origin,
                           .Group = std::move(group.Group),
                           .AvailableViews = group.AvailableViews,
                           .SurfaceIndex = Cut_.Products.size()});
    Cut_.Products.push_back(std::move(*ground));
    return true;
  }

  [[nodiscard]] std::expected<bool, std::string>
  VisitChildren(std::string_view request, uint32_t level, std::span<const size_t> known) {
    if (level >= Data::TileId::MaximumZoom) { return false; }
    std::array<const AssetRecord *, 4> partition{};
    if (known.size() == partition.size()) {
      std::ranges::transform(
          known, partition.begin(), [&](size_t child) { return Nodes_[child].Record; });
      return VisitPartition(partition);
    }
    auto indexed = Cache_.SelectTileChildren(
        {.ParentRequest = request, .Kind = kKind, .Revision = Revision_, .Level = level + 1});
    if (!indexed) { return std::unexpected("surface child metadata lookup failed"); }
    if (indexed->size() != partition.size()) { return false; }
    std::ranges::sort(*indexed, {}, &AssetRecord::RequestKey);
    std::ranges::transform(*indexed, partition.begin(), [](const auto &child) { return &child; });
    return VisitPartition(partition);
  }

  [[nodiscard]] std::expected<bool, std::string> Visit(const AssetRecord &record) {
    constexpr double kMaxErrorPx = 1;
    if (!record.RequestKey) { return false; }
    const std::string_view request = *record.RequestKey;
    const auto node = Addresses_.find(request);
    const auto known = node == Addresses_.end()
                           ? std::span<const size_t>{}
                           : std::span<const size_t>(Nodes_[node->second].Children);
    const auto error = record.Tile ? record.Tile->ErrorM : std::nullopt;
    const double distance = Clearance(record.Bounds, Demand_.SweptEyeEcef);
    const bool refine = Demand_.FocalPixels > 0 && !known.empty() &&
                        (!error || *error * Demand_.FocalPixels > distance * kMaxErrorPx);
    if (refine) {
      const auto children = VisitChildren(request, record.Level, known);
      if (!children || *children) { return children; }
    }
    const auto group = VisitGroup(record);
    if (!group || *group) { return group; }
    auto tile = ReadRecord(Cache_, Revision_, record, Cut_.RequestedBytes);
    if (!tile) { return std::unexpected(tile.error()); }
    if (!*tile) {
      if (refine) { return false; }
      return VisitChildren(request, record.Level, known);
    }
    Cut_.Products.push_back(std::move(**tile));
    return true;
  }

  AssetCache &Cache_;
  std::string_view Revision_;
  const SurfaceCutDemand &Demand_;
  std::vector<Node> Nodes_;
  std::deque<AssetRecord> Ancestors_;
  std::unordered_map<std::string_view, size_t> Addresses_;
  std::unordered_set<std::string_view> MissingParents_;
  SurfaceCutLookup Cut_;
};

}

std::string SurfaceTileRequestKey(std::string_view revision, Data::TileId tile) {
  return Request(revision, tile);
}

std::expected<void, std::string>
PublishSurfaceTile(AssetCache &cache, std::string_view revision, std::span<const uint8_t> bytes) {
  if (!AssetProductKey::Parse(revision)) {
    return std::unexpected("surface tile set needs a SHA-256 revision");
  }
  const auto tile = DecodePackedSurfaceTile(bytes);
  if (!tile) { return std::unexpected(tile.error()); }
  const auto request = Request(revision, tile->Coverage);
  const auto parent = ParentRequest(revision, tile->Coverage);
  const auto package = Sha256Hex(bytes.data(), bytes.size());
  const AssetRecord record{.Key = Sha256Hex(request + package),
                           .Kind = std::string(kKind),
                           .Bounds = EarthBounds(*tile),
                           .Package = package,
                           .ByteCount = bytes.size(),
                           .Level = static_cast<uint32_t>(tile->Coverage.Zoom),
                           .Parent = parent,
                           .RequestKey = request,
                           .Tile = AssetTile{.Revision = std::string(revision),
                                             .Coverage = Sha256Hex(Address(tile->Coverage)),
                                             .Coarser = {},
                                             .ErrorM = tile->GeometricErrorM}};
  if (!cache.Publish(std::span(&record, 1), bytes)) {
    return std::unexpected("surface tile publication failed");
  }
  return {};
}

std::expected<SurfaceTileLookup, std::string>
LoadSurfaceTileAt(AssetCache &cache, std::string_view revision, LongitudeLatitude location) {
  if (!AssetProductKey::Parse(revision)) {
    return std::unexpected("surface tile set needs a SHA-256 revision");
  }
  const auto leaf = Ground::TileIndex::Of({.LongitudeDeg = location.LongitudeDeg,
                                           .LatitudeDeg = location.LatitudeDeg},
                                          Data::TileId::MaximumZoom)
                        .Tile();
  SurfaceTileLookup result;
  if (!leaf) { return result; }
  for (auto address = *leaf; address.Zoom >= 0; address = Parent(address)) {
    const auto record = cache.FindRequest(Request(revision, address));
    if (!record) { return std::unexpected("surface tile index lookup failed"); }
    if (!*record) { continue; }
    auto tile = ReadRecord(cache, revision, **record, result.RequestedBytes);
    if (!tile) { return std::unexpected(tile.error()); }
    if (*tile) {
      result.Product = std::move(*tile);
      break;
    }
  }
  return result;
}

std::expected<SurfaceCutLookup, std::string>
LoadSurfaceCut(AssetCache &cache, std::string_view revision, const SurfaceCutDemand &demand) {
  const Vec3 radius{{demand.RadiusM, demand.RadiusM, demand.RadiusM}};
  auto records = cache.Select(
      {.Bounds = {.Min = demand.SweptEyeEcef.Min - radius, .Max = demand.SweptEyeEcef.Max + radius},
       .Kind = kKind,
       .Level = std::nullopt,
       .Radius = std::nullopt,
       .Planes = {},
       .TileRevision = revision,
       .SpatialFirst = true,
       .BoundTilesOnly = true});
  if (!records) { return std::unexpected("surface cut index query failed"); }
  std::ranges::sort(*records, {}, &AssetRecord::RequestKey);
  auto cut = CutLoader(cache, revision, demand).Load(*records);
  if (cut) { StitchSurfaceTiles(cut->Products); }
  return cut;
}

}

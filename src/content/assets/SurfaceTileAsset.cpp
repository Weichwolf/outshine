#include "SurfaceTileAsset.h"
#include "AssetCache.h"
#include "AssetPublication.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
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
    std::unordered_map<std::string_view, size_t> addresses;
    Nodes_.reserve(records.size());
    for (const auto &record : records) {
      if (!record.RequestKey || !record.Tile) { continue; }
      addresses.emplace(*record.RequestKey, Nodes_.size());
      Nodes_.push_back({.Record = &record, .Children = {}});
    }
    std::vector<size_t> roots;
    for (size_t at = 0; at < Nodes_.size(); ++at) {
      const auto &record = *Nodes_[at].Record;
      const auto parent = addresses.find(record.Parent);
      if (parent == addresses.end()) {
        roots.push_back(at);
      } else if (record.Level > 0 && Nodes_[parent->second].Record->Level == record.Level - 1) {
        Nodes_[parent->second].Children.push_back(at);
      } else {
        return std::unexpected("surface tile index contains an invalid parent");
      }
    }
    for (const size_t root : roots) {
      const auto loaded = Visit(root);
      if (!loaded) { return std::unexpected(loaded.error()); }
    }
    return std::move(Cut_);
  }

private:
  [[nodiscard]] std::expected<bool, std::string> VisitChildren(const Node &node) {
    if (node.Children.size() != 4) { return false; }
    const size_t checkpoint = Cut_.Products.size();
    for (const size_t child : node.Children) {
      const auto loaded = Visit(child);
      if (!loaded) { return std::unexpected(loaded.error()); }
      if (!*loaded) {
        Cut_.Products.resize(checkpoint);
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] std::expected<bool, std::string> Visit(size_t index) {
    constexpr double kMaxErrorPx = 1;
    const auto &node = Nodes_[index];
    const auto &record = *node.Record;
    const auto error = record.Tile ? record.Tile->ErrorM : std::nullopt;
    const double distance = Clearance(record.Bounds, Demand_.SweptEyeEcef);
    const bool refine = !error || *error * Demand_.FocalPixels > distance * kMaxErrorPx;
    if (refine) {
      const auto children = VisitChildren(node);
      if (!children || *children) { return children; }
    }
    auto tile = ReadRecord(Cache_, Revision_, record, Cut_.RequestedBytes);
    if (!tile) { return std::unexpected(tile.error()); }
    if (!*tile) {
      if (refine) { return false; }
      return VisitChildren(node);
    }
    Cut_.Products.push_back(std::move(**tile));
    return true;
  }

  AssetCache &Cache_;
  std::string_view Revision_;
  const SurfaceCutDemand &Demand_;
  std::vector<Node> Nodes_;
  SurfaceCutLookup Cut_;
};

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
  return CutLoader(cache, revision, demand).Load(*records);
}

}

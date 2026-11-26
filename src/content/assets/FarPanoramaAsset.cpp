#include "FarPanoramaAsset.h"
#include "AssetCache.h"
#include "AssetPublication.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine {
namespace {

constexpr std::string_view kKind = "render-far-panorama-v1";

std::string OriginKey(LongitudeLatitude origin) {
  const double lon = origin.LongitudeDeg == 0 ? 0 : origin.LongitudeDeg;
  const double lat = origin.LatitudeDeg == 0 ? 0 : origin.LatitudeDeg;
  return Sha256Hex(std::format("{}/{}/{}", kKind, lon, lat));
}

std::string ProfileKey(const FarPanorama &panorama) {
  auto profile =
      std::format("{}/{}/{}", panorama.Side, panorama.MaximumRangeM, panorama.ReferenceExposure);
  if (panorama.AxialNearM > 0) { profile += std::format("/{}", panorama.AxialNearM); }
  if (panorama.AxialOverlapEndM > 0) { profile += std::format("/{}", panorama.AxialOverlapEndM); }
  if (const auto &light = panorama.Lighting) {
    for (const auto &value : {light->ToSun, light->Direct, light->Sky, light->Ground}) {
      for (const float channel : value) {
        profile += std::format("/{}", channel == 0 ? 0.0f : channel);
      }
    }
  }
  return Sha256Hex(profile);
}

Vec3 ToEarth(const TangentFrame &frame, const Vec3 &eye) {
  return frame.ToEcefPosition({.EastM = eye[0], .NorthM = -eye[2], .UpM = eye[1]});
}

Box DomainBounds(const FarPanorama &panorama) {
  const auto anchor = ToEarth(TangentFrame::At(panorama.Origin), panorama.AnchorM);
  const double radius = panorama.MaximumTranslationM;
  const Vec3 reach{{radius, radius, radius}};
  return {.Min = anchor - reach, .Max = anchor + reach};
}

bool SameDomain(const Box &first, const Box &second) {
  constexpr double tolerance = PanoramaDomain::RoundoffM;
  return Length(first.Min - second.Min) <= tolerance && Length(first.Max - second.Max) <= tolerance;
}

struct Candidate {
  AssetRecord Record;
  PanoramaDomain Domain;
  std::string Profile;
};

std::expected<std::vector<Candidate>, std::string> AtEye(const AssetCache &cache,
                                                         std::string_view revision,
                                                         std::string_view origin,
                                                         const Vec3 &eye) {
  constexpr double tolerance = PanoramaDomain::RoundoffM;
  const Vec3 margin{{tolerance, tolerance, tolerance}};
  auto records = cache.Select({.Bounds = {.Min = eye - margin, .Max = eye + margin},
                               .Kind = kKind,
                               .Level = std::nullopt,
                               .Radius = std::nullopt,
                               .Planes = {},
                               .Parent = origin,
                               .TileRevision = revision,
                               .SpatialFirst = true});
  if (!records) { return std::unexpected("far panorama index query failed"); }
  std::vector<Candidate> result;
  for (auto &record : *records) {
    if (!record.Tile || record.ByteCount > FarPanorama::MaximumPackedBytes) { continue; }
    const Vec3 span = record.Bounds.Span();
    const PanoramaDomain domain{.AnchorM = record.Bounds.Middle(),
                                .RadiusM = *std::ranges::max_element(span.Axis) / 2};
    if (domain.Covers(eye)) {
      auto profile = record.Tile->Coverage;
      result.push_back(
          {.Record = std::move(record), .Domain = domain, .Profile = std::move(profile)});
    }
  }
  return result;
}

struct PanoramaSelection {
  const Candidate *First = nullptr;
  const Candidate *Next = nullptr;
};

struct Endpoints {
  Vec3 Start;
  Vec3 Finish;
};

struct EndpointCandidates {
  std::vector<Candidate> Starts;
  std::vector<Candidate> Finishes;
};

const Candidate *SingleCover(const std::vector<Candidate> &candidates, const Endpoints &eyes) {
  const Candidate *best = nullptr;
  double closest = std::numeric_limits<double>::infinity();
  for (const auto &candidate : candidates) {
    if (!candidate.Domain.Covers(eyes.Finish)) { continue; }
    const double distance = Length(candidate.Domain.AnchorM - eyes.Start);
    if (distance < closest ||
        (best != nullptr && distance == closest && candidate.Record.Key < best->Record.Key)) {
      best = &candidate;
      closest = distance;
    }
  }
  return best;
}

PanoramaSelection SelectPanoramas(const EndpointCandidates &candidates, const Endpoints &eyes) {
  if (const auto *single = SingleCover(candidates.Starts, eyes); single != nullptr) {
    return {.First = single, .Next = nullptr};
  }
  const Vec3 travel = eyes.Finish - eyes.Start;
  const double length = Length(travel);
  if (length <= PanoramaDomain::RoundoffM) { return {}; }
  const PanoramaRay ray{.OriginM = eyes.Start, .Direction = travel * (1.0 / length)};

  struct Entry {
    double Distance;
    const Candidate *Product;
  };

  std::unordered_map<std::string_view, Entry> entries;
  for (const auto &candidate : candidates.Finishes) {
    const double distance = candidate.Domain.Along(ray)[0];
    const auto [at, inserted] = entries.try_emplace(candidate.Profile, distance, &candidate);
    if (!inserted && (distance < at->second.Distance ||
                      (distance == at->second.Distance &&
                       candidate.Record.Key < at->second.Product->Record.Key))) {
      at->second = {.Distance = distance, .Product = &candidate};
    }
  }
  PanoramaSelection selection;
  double overlap = -1;
  for (const auto &candidate : candidates.Starts) {
    const auto next = entries.find(candidate.Profile);
    if (next == entries.end()) { continue; }
    const double exit = std::min(length, candidate.Domain.Along(ray)[1]);
    const double width = exit - std::max(0.0, next->second.Distance);
    if (width >= 0 && (width > overlap || (selection.First != nullptr && width == overlap &&
                                           candidate.Record.Key < selection.First->Record.Key))) {
      selection = {.First = &candidate, .Next = next->second.Product};
      overlap = width;
    }
  }
  return selection;
}

std::expected<std::optional<FarPanorama>, std::string>
Read(AssetCache &cache, const Candidate &candidate, size_t &requestedBytes) {
  requestedBytes += static_cast<size_t>(candidate.Record.ByteCount);
  std::optional<FarPanorama> panorama;
  const auto loaded = cache.LoadValidated(
      candidate.Record.Key,
      kKind,
      FarPanorama::MaximumPackedBytes,
      [&](const CachedAsset &asset) -> std::expected<bool, AssetCacheError> {
        auto decoded = DecodeFarPanorama(asset.Bytes());
        if (!decoded || !SameDomain(DomainBounds(*decoded), candidate.Record.Bounds) ||
            ProfileKey(*decoded) != candidate.Profile ||
            OriginKey(decoded->Origin) != candidate.Record.Parent) {
          return false;
        }
        panorama = std::move(*decoded);
        return true;
      });
  if (!loaded && loaded.error() != AssetCacheError::CapacityExceeded) {
    return std::unexpected("far panorama package lookup failed");
  }
  return panorama;
}

}

std::expected<void, std::string>
PublishFarPanorama(AssetCache &cache, std::string_view revision, const FarPanorama &panorama) {
  if (!AssetProductKey::Parse(revision)) {
    return std::unexpected("far panorama set needs a SHA-256 revision");
  }
  auto bytes = EncodeFarPanorama(panorama);
  if (!bytes) { return std::unexpected(bytes.error()); }
  const auto package = Sha256Hex(bytes->data(), bytes->size());
  const auto key = Sha256Hex(std::format("{}/{}/{}", kKind, revision, package));
  const AssetRecord record{.Key = key,
                           .Kind = std::string(kKind),
                           .Bounds = DomainBounds(panorama),
                           .Package = package,
                           .ByteCount = bytes->size(),
                           .Parent = OriginKey(panorama.Origin),
                           .Tile = AssetTile{.Revision = std::string(revision),
                                             .Coverage = ProfileKey(panorama),
                                             .Coarser = {}}};
  if (!cache.Publish(std::span(&record, 1), *bytes)) {
    return std::unexpected("far panorama publication failed");
  }
  return {};
}

std::expected<FarPanoramaLookup, std::string> LoadFarPanoramas(AssetCache &cache,
                                                               std::string_view revision,
                                                               LongitudeLatitude origin,
                                                               const Vec3 &startM,
                                                               const Vec3 &finishM) {
  const auto frame = TangentFrame::At(origin);
  const auto key = OriginKey(origin);
  const Endpoints eyes{.Start = ToEarth(frame, startM), .Finish = ToEarth(frame, finishM)};
  auto starts = AtEye(cache, revision, key, eyes.Start);
  if (!starts) { return std::unexpected(starts.error()); }
  EndpointCandidates candidates{.Starts = std::move(*starts), .Finishes = {}};
  if (eyes.Start != eyes.Finish) {
    auto found = AtEye(cache, revision, key, eyes.Finish);
    if (!found) { return std::unexpected(found.error()); }
    candidates.Finishes = std::move(*found);
  }
  if (candidates.Starts.empty() && candidates.Finishes.empty()) { return FarPanoramaLookup{}; }
  FarPanoramaLookup result;
  for (;;) {
    const auto selection = SelectPanoramas(candidates, eyes);
    if (selection.First == nullptr) {
      return std::unexpected("no indexed far panorama covers the flight");
    }
    result.Products.clear();
    std::string failed;
    for (const auto *candidate : {selection.First, selection.Next}) {
      if (candidate == nullptr) { break; }
      auto product = Read(cache, *candidate, result.RequestedBytes);
      if (!product) { return std::unexpected(product.error()); }
      if (!*product) {
        failed = candidate->Record.Key;
        break;
      }
      result.Products.push_back(std::move(**product));
    }
    if (failed.empty()) { return result; }
    const auto discard = [&](const Candidate &candidate) { return candidate.Record.Key == failed; };
    std::erase_if(candidates.Starts, discard);
    std::erase_if(candidates.Finishes, discard);
  }
}

}

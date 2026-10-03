#ifndef OUTSHINE_WORLD_DATA_SOURCEDECL_H
#define OUTSHINE_WORLD_DATA_SOURCEDECL_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "Address.h"
#include "DataKind.h"
#include <world/SourceProvider.h>

namespace outshine::Data {

/// Ordering within one native data category; lower ranks are attempted first.
enum class Rank : int32_t {};

/// Missing-source action used by the shared source scheduler.
using AbsencePolicy = MissingDataPolicy;

/// Source-byte encoding consumed by the corresponding native adapter.
enum class WireFormat : uint8_t {
  TerrariumPng,     ///< RGB Terrarium height encoding; retained for explicit fixtures.
  MapboxVectorTile, ///< Mapbox vector tile protobuf encoding.
  StarBandBinary,   ///< Built-in star catalogue band encoding.
  OsmXml,           ///< Original OSM XML including nodes, ways, relations and tags.
  CopernicusCog,    ///< Original Copernicus GLO-30 Float32 GeoTIFF byte intervals.
  TerrariumWebp,    ///< Lossless RGB Terrarium height encoding in a WebP container.
  ProviderDefined   ///< Source-owned encoding, normalized through its native decoding contract.
};
/// Describe a WireFormat value without allocation.
/// @param wire Value to describe.
/// @return Process-lifetime name, or empty for an unsupported value.
[[nodiscard]] const char *Name(WireFormat wire) noexcept;

/// Acquisition class used for scheduling and provider diagnostics.
enum class LatencyClass : uint8_t {
  Local,    ///< Locally available source; does not imply zero IO cost.
  Regional, ///< Regional acquisition.
  Distant   ///< Remote acquisition.
};
/// Describe a LatencyClass value without allocation.
/// @param latency Value to describe.
/// @return Process-lifetime name, or empty for an unsupported value.
[[nodiscard]] const char *Name(LatencyClass latency) noexcept;

/// Persistent source-byte storage policy; generated runtime products are separate.
enum class Cacheability : uint8_t {
  Never,  ///< Do not persist received bytes.
  Forever ///< Persist received source bytes under the declared cache identity.
};
/// Describe a Cacheability value without allocation.
/// @param keeps Value to describe.
/// @return Process-lifetime name, or empty for an unsupported value.
[[nodiscard]] const char *Name(Cacheability keeps) noexcept;

/// Whether unavailable content is essential to the requested world.
enum class Necessity : uint8_t {
  Cosmetic, ///< Optional cosmetic data.
  Required  ///< Required data; absence remains visible.
};
/// Describe a Necessity value without allocation.
/// @param need Value to describe.
/// @return Process-lifetime name, or empty for an unsupported value.
[[nodiscard]] const char *Name(Necessity need) noexcept;

/// Refinement fallback after a confirmed missing tile from the selected source.
enum class TileAbsencePolicy : uint8_t {
  SourcePolicy, ///< Apply OnAbsent without changing the served tile.
  Parent        ///< Try ancestors down to MinZoom before applying OnAbsent.
};

/// Owned immutable identity and scheduling settings for one configured source.
/// Source owns this value; cache keys include its identity, revision and endpoint.
/// No method here performs IO. Sizes are bytes, retry counts are dimensionless.
struct SourceDecl {
  std::string Id; ///< Stable nonempty source ID.

  uint32_t Version = 1; ///< Source schema version participating in cache identity.
  std::string Revision; ///< Dataset revision, owned independently of downloaded payload bytes.
  std::string Endpoint; ///< Configured endpoint/location participating in cache identity.

  DataKind Kind = DataKind::Elevation;        ///< Native consumer category.
  Scheme How = Scheme::TileZxy;               ///< Family of the addresses actually served.
  WireFormat Wire = WireFormat::TerrariumPng; ///< Encoding of returned source bytes.

  Rank Order = Rank{0};                             ///< Attempt order within Kind.
  AbsencePolicy OnAbsent = AbsencePolicy::Continue; ///< Action after authoritative source absence.

  int MinZoom = 0; ///< Lowest admitted tile zoom; ignored for other address families.
  int MaxZoom = 0; ///< Finest source tile zoom; render detail is independent.

  bool AncestorFill = false; ///< Whether higher-zoom requests can consume a declared ancestor.

  Cacheability Keeps =
      Cacheability::Forever;            ///< Persistent storage policy for received source bytes.
  Necessity Need = Necessity::Required; ///< Required or cosmetic content.
  LatencyClass Latency = LatencyClass::Distant; ///< Declared acquisition class.

  size_t TypicalPayloadBytes = 0; ///< Planning estimate; does not grant capacity.

  int RetryBudget = 0; ///< Maximum retries after the initial attempt.
  size_t MaximumPayloadBytes =
      0;                     ///< Enforced response/cache-read byte cap; zero uses shared defaults.
  std::string PayloadSha256; ///< Optional lowercase SHA-256 digest required of returned bytes.
  std::string Schema; ///< Source-owned semantic schema; interpreted by its generator extension.
  TileAbsencePolicy TileAbsence =
      TileAbsencePolicy::SourcePolicy; ///< Explicit tile refinement policy.
};

}
#endif

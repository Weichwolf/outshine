#ifndef SOURCEDECL_H
#define SOURCEDECL_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

/* The order sources are asked in within one kind, declared at registration. No enumerators: which
 * ranks exist is a declaration, and naming them here would put a list of upstreams in the engine. */
enum class Rank : uint16_t {};

/* WHAT COMES OFF THE WIRE, and the decoder is registered against THIS rather than against the
 * source: a second elevation upstream in the same format is a registration and not an edit inside a
 * decoder. */
enum class WireFormat : uint8_t { TerrariumPng, MapboxVectorTile, StarBandBinary };
[[nodiscard]] const char *Name(WireFormat wire) noexcept;

/* WHAT A ROUND TRIP COSTS BEFORE ONE IS TAKEN. Declared, and published beside the measured one, so a
 * declaration that has drifted from the wire is a visible disagreement and not a comment. */
enum class LatencyClass : uint8_t { Local, Regional, Distant };
[[nodiscard]] const char *Name(LatencyClass latency) noexcept;

/* WHETHER A DELIVERY MAY BE KEPT, and it is declared rather than inferred. A DEM tile is bedrock; a
 * forecast expires. A store that cannot tell them apart either re-fetches bedrock or serves
 * yesterday's wind. */
enum class Cacheability : uint8_t { Never, Forever };
[[nodiscard]] const char *Name(Cacheability keeps) noexcept;

/* WHETHER THE WORLD IS INCOMPLETE WITHOUT IT. A missing imagery tile is cosmetic and a missing DEM
 * tile is not, and today both travel as the same absence with only the call site knowing which. */
enum class Necessity : uint8_t { Cosmetic, Required };
[[nodiscard]] const char *Name(Necessity need) noexcept;

/* WHAT ONE UPSTREAM SAYS ABOUT ITSELF BEFORE IT IS ASKED ANYTHING.
 *
 * Every field is answerable with no network and no work, which is what lets the selector choose
 * before any is done — the same property that makes a generator's Proposes(areaM2) a pure predicate.
 * The one field that is not a fact about the upstream is Version, and it is what the content key
 * covers: a source that changes the bytes it produces without raising it will serve its old cache
 * entries for ever, and that is the one failure the store cannot catch. */
struct SourceDecl {
  /* Stable, and it enters the content key. Renaming it invalidates the store, which is correct. */
  std::string Id;
  /* Raised whenever this source's produced bytes change meaning for an unchanged address. */
  uint32_t Version = 1;

  DataKind Kind = DataKind::Elevation;
  Scheme How = Scheme::TileZxy;
  WireFormat Wire = WireFormat::TerrariumPng;
  /* Where this source stands among the sources of its kind. Unique within a kind, and a duplicate is
   * refused at registration rather than settled by whoever answers first. */
  Rank Order = Rank{0};

  /* The zoom band this source may be ASKED for. It exists here and nowhere else: the three copies of
   * `15` this replaced disagreed with a fourth number that had no stated relation to them. */
  int MinZoom = 0;
  int MaxZoom = 0;
  /* Whether a request above MaxZoom is answered from the nearest ancestor within the band. A raster
   * can be cropped from its parent; a vector tile cannot, so this is per source and not global. */
  bool AncestorFill = false;

  Cacheability Keeps = Cacheability::Forever;
  Necessity Need = Necessity::Required;
  LatencyClass Latency = LatencyClass::Distant;
  /* Declared payload, bytes. Published beside the measured mean on the ordinary telemetry row. */
  size_t TypicalPayloadBytes = 0;
  /* How many times a Retry verdict is worth repeating before this source is treated as refusing.
   * Per source, because a transient 5xx from a bucket and a rate limit are different conversations. */
  int RetryBudget = 0;
};

} // namespace outshine::Data
#endif

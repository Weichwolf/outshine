#ifndef OUTSHINE_DATA_SOURCEDECL_H
#define OUTSHINE_DATA_SOURCEDECL_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

enum class Rank : uint16_t {};

enum class WireFormat : uint8_t { TerrariumPng, MapboxVectorTile, StarBandBinary };
[[nodiscard]] const char *Name(WireFormat wire) noexcept;

enum class LatencyClass : uint8_t { Local, Regional, Distant };
[[nodiscard]] const char *Name(LatencyClass latency) noexcept;

enum class Cacheability : uint8_t { Never, Forever };
[[nodiscard]] const char *Name(Cacheability keeps) noexcept;

enum class Necessity : uint8_t { Cosmetic, Required };
[[nodiscard]] const char *Name(Necessity need) noexcept;

struct SourceDecl {

  std::string Id;

  uint32_t Version = 1;

  DataKind Kind = DataKind::Elevation;
  Scheme How = Scheme::TileZxy;
  WireFormat Wire = WireFormat::TerrariumPng;

  Rank Order = Rank{0};

  int MinZoom = 0;
  int MaxZoom = 0;

  bool AncestorFill = false;

  Cacheability Keeps = Cacheability::Forever;
  Necessity Need = Necessity::Required;
  LatencyClass Latency = LatencyClass::Distant;

  size_t TypicalPayloadBytes = 0;

  int RetryBudget = 0;
};

}
#endif

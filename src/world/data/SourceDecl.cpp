#include "SourceDecl.h"

namespace outshine::Data {

const char *Name(WireFormat wire) noexcept {
  switch (wire) {
    case WireFormat::TerrariumPng: return "terrarium-png";
    case WireFormat::MapboxVectorTile: return "mvt";
    case WireFormat::StarBandBinary: return "star-band";
  }
  return "";
}

const char *Name(LatencyClass latency) noexcept {
  switch (latency) {
    case LatencyClass::Local: return "local";
    case LatencyClass::Regional: return "regional";
    case LatencyClass::Distant: return "distant";
  }
  return "";
}

const char *Name(Cacheability keeps) noexcept {
  switch (keeps) {
    case Cacheability::Never: return "never";
    case Cacheability::Forever: return "forever";
  }
  return "";
}

const char *Name(Necessity need) noexcept {
  switch (need) {
    case Necessity::Cosmetic: return "cosmetic";
    case Necessity::Required: return "required";
  }
  return "";
}

}

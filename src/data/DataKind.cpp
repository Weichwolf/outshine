#include "DataKind.h"

namespace outshine::Data {

const char *Name(DataKind kind) noexcept {
  switch (kind) {
    case DataKind::Elevation: return "elevation";
    case DataKind::VectorMap: return "vector";
    case DataKind::StarCatalogue: return "stars";
  }
  return "";
}

}

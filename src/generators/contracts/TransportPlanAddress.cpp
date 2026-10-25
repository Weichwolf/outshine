#include "TransportPlanAddress.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <cstdint>
#include <expected>
#include <string>

namespace outshine::Generators {
std::expected<std::string, TransportPlanRequestError>
TransportPlanRequestKey(const TransportPlanAddress &address) {
  if (!address.Core.Valid()) { return std::unexpected(TransportPlanRequestError::InvalidCore); }
  if (address.Representation != TransportRepresentation::Functional ||
      address.RepresentationVersion != 1) {
    return std::unexpected(TransportPlanRequestError::UnsupportedRepresentation);
  }
  BinaryValueWriter out(256);
  constexpr uint32_t recipeVersion = 1;
  if (!out.Text("outshine/functional-transport-plan") ||
      !out(recipeVersion,
           static_cast<uint32_t>(address.Core.Level),
           address.Core.X,
           address.Core.Y,
           address.Inputs.Bytes,
           address.Representation,
           address.RepresentationVersion)) {
    return std::unexpected(TransportPlanRequestError::EncodingFailed);
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}
}

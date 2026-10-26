#include "TransportPlanAddress.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <cstdint>
#include <expected>
#include <string>

namespace outshine::Generators {
std::expected<std::string, TransportPlanRequestError>
TransportPlanRequestKey(const TransportPlanAddress &address) {
  const auto &definition = address.Definition;
  if (!definition.Core.Valid()) { return std::unexpected(TransportPlanRequestError::InvalidCore); }
  if (definition.Representation != TransportRepresentation::Functional ||
      definition.RepresentationVersion != 2) {
    return std::unexpected(TransportPlanRequestError::UnsupportedRepresentation);
  }
  BinaryValueWriter out(256);
  if (!out.Text("outshine/functional-transport-plan") ||
      !out(kTransportPlanRecipeVersion,
           static_cast<uint32_t>(definition.Core.Level),
           definition.Core.X,
           definition.Core.Y,
           address.Inputs.Bytes,
           definition.Representation,
           definition.RepresentationVersion)) {
    return std::unexpected(TransportPlanRequestError::EncodingFailed);
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}
}

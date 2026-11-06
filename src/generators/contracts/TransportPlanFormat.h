#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANFORMAT_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANFORMAT_H

#include "BinaryValueArchive.h"
#include "BudgetedBinaryValueReader.h"
#include "RoadValueFields.h"
#include "TransportPlanCodec.h"
#include "TransportProvenance.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::TransportFormat {
inline constexpr std::string_view InputsDomain = "outshine/transport-inputs/1";

using Reader = BudgetedBinaryValueReader<TransportPlanCodecError>;

template <class T, class Write>
bool List(BinaryValueWriter &out, std::span<const T> items, Write write) {
  return items.size() <= std::numeric_limits<uint32_t>::max() &&
         out(static_cast<uint32_t>(items.size())) && std::ranges::all_of(items, write);
}

bool Network(BinaryValueWriter &out,
             const std::shared_ptr<const World::TransportNetworkSnapshot> &network);
bool Network(Reader &in, std::shared_ptr<const World::TransportNetworkSnapshot> &network);
bool Provenance(BinaryValueWriter &out, const World::TransportProvenance &provenance);
bool Provenance(Reader &in, World::TransportProvenance &provenance);
bool Input(BinaryValueWriter &out, const TransportInputReference &input);
bool Input(Reader &in, TransportInputReference &input);
bool Path(BinaryValueWriter &out, const TransportPath &path);
bool Path(Reader &in, TransportPath &path);
bool Port(BinaryValueWriter &out, const TransportPort &port);
bool Port(Reader &in, TransportPort &port);
bool Junction(BinaryValueWriter &out, const TransportJunction &junction);
bool Junction(Reader &in, TransportJunction &junction);
using RoadValueFields::Body;
using RoadValueFields::Gate;
using RoadValueFields::Station;
bool Contact(BinaryValueWriter &out, const EarthworkStamp &stamp);
bool Contact(Reader &in, EarthworkStamp &stamp);
bool Omission(BinaryValueWriter &out, const TransportOmission &omission);
bool Omission(Reader &in, TransportOmission &omission);
bool InputsValid(std::span<const TransportInputReference> inputs);
bool NetworkInputsBound(const TransportPlan &plan);
bool ValidProfile(RoadProfile profile);
bool ValidUse(TransportUse use);
bool ValidGate(const RoadGate &gate);
bool ValidPort(const TransportPort &port, GeoCellId core);
using RoadValueFields::ValidBody;
bool ValidContact(const EarthworkStamp &stamp);
}
#endif

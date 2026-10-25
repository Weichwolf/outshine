#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANFORMAT_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANFORMAT_H

#include "BinaryValueArchive.h"
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

class Reader {
public:
  Reader(std::span<const uint8_t> bytes, size_t capacity) : Values_(bytes), Remaining_(capacity) {}

  template <class... T> bool operator()(T &...value) { return Values_(value...); }

  [[nodiscard]] size_t RemainingBytes() const noexcept { return Values_.In.Remaining(); }

  [[nodiscard]] std::optional<std::span<const uint8_t>> TakeBytes(size_t count) {
    return Values_.In.Take(count);
  }

  [[nodiscard]] TransportPlanCodecError Failure() const noexcept { return Error_; }

  [[nodiscard]] bool Charge(size_t bytes) noexcept {
    if (bytes > Remaining_) {
      Error_ = TransportPlanCodecError::CapacityExceeded;
      return false;
    }
    Remaining_ -= bytes;
    return true;
  }

  bool Text(std::string &text) {
    uint32_t count = 0;
    if (!Values_(count)) { return false; }
    const auto bytes = TakeBytes(count);
    if (!bytes || !Charge(static_cast<size_t>(count) + 1)) { return false; }
    text.assign(reinterpret_cast<const char *>(bytes->data()), bytes->size());
    return text.capacity() <= count || Charge(text.capacity() - count);
  }

  template <class T, class Read> bool List(std::vector<T> &items, size_t wireBytes, Read read) {
    uint32_t count = 0;
    if (!Values_(count) || count > RemainingBytes() / wireBytes) { return false; }
    if (count > Remaining_ / sizeof(T) || !Charge(static_cast<size_t>(count) * sizeof(T))) {
      Error_ = TransportPlanCodecError::CapacityExceeded;
      return false;
    }
    items.resize(count);
    return (items.capacity() == count || Charge((items.capacity() - count) * sizeof(T))) &&
           std::ranges::all_of(items, read);
  }

private:
  BinaryValueReader Values_;
  size_t Remaining_;
  TransportPlanCodecError Error_ = TransportPlanCodecError::Malformed;
};

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
bool Station(BinaryValueWriter &out, const RoadStation &station);
bool Station(Reader &in, RoadStation &station);
bool Gate(BinaryValueWriter &out, const RoadGate &gate);
bool Gate(Reader &in, RoadGate &gate);
bool Body(BinaryValueWriter &out, const HeightSolid &body);
bool Body(Reader &in, HeightSolid &body);
bool Contact(BinaryValueWriter &out, const EarthworkStamp &stamp);
bool Contact(Reader &in, EarthworkStamp &stamp);
bool Omission(BinaryValueWriter &out, const TransportOmission &omission);
bool Omission(Reader &in, TransportOmission &omission);
bool InputsValid(std::span<const TransportInputReference> inputs);
bool ValidProfile(RoadProfile profile);
bool ValidUse(TransportUse use);
bool ValidGate(const RoadGate &gate);
bool ValidPort(const TransportPort &port, GeoCellId core);
bool ValidBody(const HeightSolid &body);
bool ValidContact(const EarthworkStamp &stamp);
}
#endif

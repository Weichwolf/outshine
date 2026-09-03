#include "StarBands.h"

#include <array>
#include <cstdint>
#include <optional>
#include <cstdio>
#include <utility>
#include <vector>

namespace outshine::Data {

constexpr size_t kTypicalPayloadBytes = 13380;

namespace {

[[nodiscard]] SourceDecl Declared() {
  SourceDecl d;
  d.Id = "hyg.bands";

  d.Version = 1;
  d.Kind = DataKind::StarCatalogue;
  d.How = Scheme::WholeWorld;
  d.Wire = WireFormat::StarBandBinary;
  d.Order = Rank{0};
  d.Keeps = Cacheability::Never;
  d.Need = Necessity::Required;
  d.Latency = LatencyClass::Local;

  d.TypicalPayloadBytes = kTypicalPayloadBytes;
  d.RetryBudget = 0;
  return d;
}

} // namespace

StarBands::StarBands(std::string directory) : Directory_(std::move(directory)), Decl_(Declared()) {}

Coverage StarBands::Covers(const Fetch &request) const noexcept {
  if (request.Kind() != Decl_.Kind) { return Coverage::Outside; }
  const std::optional<uint32_t> band = request.Where().Index();
  if (!band) { return Coverage::Outside; }
  return *band < kBands ? Coverage::Inside : Coverage::Outside;
}

Ticket StarBands::Begin(const Address &at, Transport &transport) const {
  (void)at;
  (void)transport;
  return Ticket::None;
}

Fetched StarBands::Collect(const Address &at, Ticket ticket, Transport &transport) const {
  (void)ticket;
  (void)transport;
  const std::optional<uint32_t> band = at.Index();
  if (!band) { return Fetched::Meant(Meaning::Refused); }
  std::array<char, 512> path{};
  std::snprintf(path.data(), path.size(), "%s/band%u.bin", Directory_.c_str(), *band);
  std::FILE *f = std::fopen(path.data(), "rb");
  if (f == nullptr) { return Fetched::Meant(Meaning::Refused); }
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> bytes;
  bool whole = size > 0;
  if (whole) {
    bytes.resize(static_cast<size_t>(size));
    whole = std::fread(bytes.data(), 1, static_cast<size_t>(size), f) == static_cast<size_t>(size);
  }
  std::fclose(f);
  if (!whole) { return Fetched::Meant(Meaning::Refused); }
  return Fetched::Delivered(std::move(bytes));
}

} // namespace outshine::Data

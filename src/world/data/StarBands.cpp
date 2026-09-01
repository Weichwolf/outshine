#include "StarBands.h"

#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

namespace outshine::Data {
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

  d.TypicalPayloadBytes = 13380;
  d.RetryBudget = 0;
  return d;
}

} // namespace

StarBands::StarBands(std::string directory) : Directory_(std::move(directory)), Decl_(Declared()) {}

Coverage StarBands::Covers(const Request &request) const noexcept {
  if (request.Kind() != Decl_.Kind) { return Coverage::Outside; }
  uint32_t band = 0;
  if (!request.Where().TryIndex(&band)) { return Coverage::Outside; }
  return band < kBands ? Coverage::Inside : Coverage::Outside;
}

Ticket StarBands::Begin(const Address &at, Transport &transport) const {
  (void)at;
  (void)transport;
  return Ticket::None;
}

Fetched StarBands::Collect(const Address &at, Ticket ticket, Transport &transport) const {
  (void)ticket;
  (void)transport;
  uint32_t band = 0;
  if (!at.TryIndex(&band)) { return Fetched::Meant(Meaning::Refused); }
  char path[512];
  std::snprintf(path, sizeof path, "%s/band%u.bin", Directory_.c_str(), band);
  std::FILE *f = std::fopen(path, "rb");
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

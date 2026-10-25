#include "StarCatalogueSource.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <cstdio>
#include <utility>
#include <vector>

namespace outshine::Generators::Sky {

constexpr size_t kTypicalPayloadBytes = 13380;

namespace {

[[nodiscard]] Data::SourceDecl
Declared(std::string revision, Data::Rank order, Data::AbsencePolicy absence) {
  Data::SourceDecl d;
  d.Id = "hyg.bands";

  d.Version = 1;
  d.Revision = std::move(revision);
  d.Kind = Data::DataKind::StarCatalogue;
  d.How = Data::Scheme::WholeWorld;
  d.Wire = Data::WireFormat::StarBandBinary;
  d.Order = order;
  d.OnAbsent = absence;
  d.Keeps = Data::Cacheability::Never;
  d.Need = Data::Necessity::Required;
  d.Latency = Data::LatencyClass::Local;

  d.TypicalPayloadBytes = kTypicalPayloadBytes;
  d.RetryBudget = 0;
  return d;
}

}

StarCatalogueSource::StarCatalogueSource(std::string directory,
                                         std::string revision,
                                         Data::Rank order,
                                         Data::AbsencePolicy absence)
    : Directory_(std::move(directory)), Decl_(Declared(std::move(revision), order, absence)) {}

Data::Coverage StarCatalogueSource::Covers(const Data::SourceRequest &request) const noexcept {
  if (request.Kind() != Decl_.Kind) { return Data::Coverage::Outside; }
  const std::optional<uint32_t> band = request.Where().Index();
  if (!band) { return Data::Coverage::Outside; }
  return *band < kBands ? Data::Coverage::Inside : Data::Coverage::Outside;
}

Data::FetchStart StarCatalogueSource::Begin(const Data::Address &at,
                                            Data::Transport &transport) const {
  (void)at;
  (void)transport;
  return Data::Ticket::None;
}

Data::SourceResponse StarCatalogueSource::Collect(const Data::Address &at,
                                                  Data::Ticket ticket,
                                                  Data::Transport &transport) const {
  (void)ticket;
  (void)transport;
  const std::optional<uint32_t> band = at.Index();
  if (!band) { return Data::SourceResponse::Meant(Data::Meaning::Refused); }
  std::array<char, 512> path{};
  std::snprintf(path.data(), path.size(), "%s/band%u.bin", Directory_.c_str(), *band);
  std::FILE *f = std::fopen(path.data(), "rb");
  if (f == nullptr) { return Data::SourceResponse::Meant(Data::Meaning::Refused); }
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
  if (!whole) { return Data::SourceResponse::Meant(Data::Meaning::Refused); }
  return Data::SourceResponse::Delivered(std::move(bytes));
}

}

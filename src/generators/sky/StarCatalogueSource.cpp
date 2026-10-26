#include "StarCatalogueSource.h"

#include <cstdint>
#include <cstddef>
#include <ios>
#include <optional>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Sky {

constexpr size_t kTypicalPayloadBytes = 13380;
constexpr size_t kMaximumBandBytes = size_t{1024} * 1024u;

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
  d.MaximumPayloadBytes = kMaximumBandBytes;
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
                                            Data::SourceTransport &transport) const {
  (void)at;
  (void)transport;
  return Data::Ticket::None;
}

Data::SourceResponse
StarCatalogueSource::Collect(const Data::Address &at,
                             [[maybe_unused]] Data::Ticket ticket,
                             [[maybe_unused]] Data::SourceTransport &transport) const {
  const auto band = at.Index();
  if (!band || *band >= kBands || Directory_.contains('\0')) {
    return Data::SourceResponse::Meant(Data::Meaning::Refused,
                                       Data::SourceFailureReason::InvalidRequest);
  }
  const auto path = std::filesystem::path(Directory_) / std::format("band{}.bin", *band);
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return Data::SourceResponse::Meant(Data::Meaning::Refused,
                                       Data::SourceFailureReason::Unavailable);
  }
  const std::streamoff size = file.tellg();
  if (size <= 0) {
    return Data::SourceResponse::Meant(Data::Meaning::Refused,
                                       Data::SourceFailureReason::CorruptPayload);
  }
  if (std::cmp_greater(size, Decl_.MaximumPayloadBytes)) {
    return Data::SourceResponse::Meant(Data::Meaning::Refused,
                                       Data::SourceFailureReason::CapacityRefused);
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  if (!file.read(reinterpret_cast<char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()))) {
    return Data::SourceResponse::Meant(Data::Meaning::Refused,
                                       Data::SourceFailureReason::CorruptPayload);
  }
  const auto next = file.peek();
  if (file.bad() || next != std::char_traits<char>::eof()) {
    return Data::SourceResponse::Meant(Data::Meaning::Refused,
                                       Data::SourceFailureReason::CorruptPayload);
  }
  return Data::SourceResponse::Delivered(std::move(bytes));
}

}

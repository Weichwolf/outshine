#include "CopernicusDem.h"

#include <cstdlib>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Data {
namespace {
constexpr size_t kMostBlockBytes = size_t{8} * 1024 * 1024;
constexpr size_t kHeaderBytes = 16384;
constexpr int kHttpNotFound = 404, kHttpPreconditionFailed = 412, kHttpTimeout = 408;
constexpr int kHttpTooMany = 429, kHttpServerFirst = 500;
constexpr size_t kLatitudeDigits = 2, kLongitudeDigits = 3;
constexpr int kLatitudeLimit = 90, kLongitudeLimit = 180;

bool ValidCell(CellId cell) {
  return cell.SouthDeg >= -kLatitudeLimit && cell.SouthDeg < kLatitudeLimit &&
         cell.WestDeg >= -kLongitudeLimit && cell.WestDeg < kLongitudeLimit;
}

std::string Coordinate(int degrees, std::string_view hemispheres, size_t width) {
  const auto digits = std::to_string(std::abs(degrees));
  std::string coordinate(1, hemispheres[degrees < 0 ? 1 : 0]);
  coordinate.append(width - digits.size(), '0');
  coordinate += digits;
  return coordinate;
}
}

CopernicusDem::CopernicusDem(std::string revision,
                             Rank order,
                             AbsencePolicy absence,
                             std::string dataset) {
  Decl_.Id = std::move(dataset);
  Decl_.Revision = std::move(revision);
  Decl_.Endpoint = "https://copernicus-dem-30m.s3.amazonaws.com/";
  Decl_.Kind = DataKind::Elevation;
  Decl_.How = Scheme::GeographicCell;
  Decl_.Wire = WireFormat::CopernicusCog;
  Decl_.Order = order;
  Decl_.OnAbsent = absence;
  Decl_.TypicalPayloadBytes = kHeaderBytes;
  Decl_.MaximumPayloadBytes = kMostBlockBytes;
  Decl_.RetryBudget = 2;
}

Coverage CopernicusDem::Covers(const Fetch &request) const noexcept {
  const auto cell = request.Where().Cell();
  return request.Kind() == DataKind::Elevation && request.Range() && cell && ValidCell(*cell)
             ? Coverage::Inside
             : Coverage::Outside;
}

std::string CopernicusDem::Url(const Address &at) const {
  const auto cell = at.Cell();
  if (!cell || !ValidCell(*cell)) { return {}; }
  const std::string name = "Copernicus_DSM_COG_10_" +
                           Coordinate(cell->SouthDeg, "NS", kLatitudeDigits) + "_00_" +
                           Coordinate(cell->WestDeg, "EW", kLongitudeDigits) + "_00_DEM";
  return Decl_.Endpoint + name + '/' + name + ".tif";
}

FetchStart CopernicusDem::Begin([[maybe_unused]] const Address &at,
                                [[maybe_unused]] Transport &transport) const {
  return std::unexpected(FetchFailureReason::InvalidRequest);
}

FetchStart CopernicusDem::Begin(const Fetch &request, Transport &transport) const {
  const auto &range = request.Range();
  if (!range || Covers(request) != Coverage::Inside || !range->Valid() ||
      (!request.EntityTag().empty() && !StrongEntityTag(request.EntityTag()))) {
    return std::unexpected(FetchFailureReason::InvalidRequest);
  }
  if (range->Length > Decl_.MaximumPayloadBytes) {
    return std::unexpected(FetchFailureReason::CapacityRefused);
  }
  return transport.Begin(Url(request.Where()), *range, request.EntityTag());
}

Fetched CopernicusDem::Collect([[maybe_unused]] const Address &at,
                               Ticket ticket,
                               Transport &transport) const {
  auto wire = transport.Collect(ticket);
  switch (wire.Where()) {
    case Wire::State::Working: return Fetched::Working();
    case Wire::State::Unreachable: {
      const auto reason = wire.FailureReason();
      const bool terminal =
          reason == FetchFailureReason::Cancelled || reason == FetchFailureReason::CapacityRefused;
      return Fetched::Meant(terminal ? Meaning::Refused : Meaning::Retry, reason);
    }
    case Wire::State::Consumed:
    case Wire::State::Never: return Fetched::Meant(Meaning::Refused, wire.FailureReason());
    case Wire::State::Answered: break;
  }
  auto response = wire.Take();
  if (!response) { return Fetched::Meant(Meaning::Refused); }
  if (response->Status == kHttpNotFound) { return Fetched::NotFound(); }
  if (response->Status == kHttpPreconditionFailed) {
    return Fetched::Meant(Meaning::Refused, FetchFailureReason::SourceChanged);
  }
  if (response->Status == kHttpTimeout || response->Status == kHttpTooMany ||
      response->Status >= kHttpServerFirst) {
    return Fetched::MeantAfter(Meaning::Retry,
                               wire.RetryAfterS(),
                               response->Status == kHttpTimeout ? FetchFailureReason::TimedOut
                                                                : FetchFailureReason::Unavailable);
  }
  if (response->Status != kHttpPartialContent || !response->Range ||
      !response->Range->Valid(response->Body.size())) {
    return Fetched::Meant(Meaning::Refused, FetchFailureReason::CorruptPayload);
  }
  return Fetched::Delivered(std::move(response->Body), std::move(response->Range));
}

}

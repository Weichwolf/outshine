#include "CopernicusDemSource.h"

#include <cstdlib>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Generators::Terrain {
namespace {
constexpr size_t kMostBlockBytes = size_t{8} * 1024 * 1024;
constexpr size_t kHeaderBytes = 16384;
constexpr int kHttpNotFound = 404, kHttpPreconditionFailed = 412, kHttpTimeout = 408;
constexpr int kHttpTooMany = 429, kHttpServerFirst = 500;
constexpr size_t kLatitudeDigits = 2, kLongitudeDigits = 3;
constexpr int kLatitudeLimit = 90, kLongitudeLimit = 180;

bool ValidCell(Data::CellId cell) {
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

CopernicusDemSource::CopernicusDemSource(std::string revision,
                                         Data::Rank order,
                                         Data::AbsencePolicy absence,
                                         std::string dataset) {
  Decl_.Id = std::move(dataset);
  Decl_.Revision = std::move(revision);
  Decl_.Endpoint = Endpoint;
  Decl_.Kind = Data::DataKind::Elevation;
  Decl_.How = Data::Scheme::GeographicCell;
  Decl_.Wire = Data::WireFormat::CopernicusCog;
  Decl_.Order = order;
  Decl_.OnAbsent = absence;
  Decl_.TypicalPayloadBytes = kHeaderBytes;
  Decl_.MaximumPayloadBytes = kMostBlockBytes;
  Decl_.RetryBudget = 2;
}

Data::Coverage CopernicusDemSource::Covers(const Data::SourceRequest &request) const noexcept {
  const auto cell = request.Where().Cell();
  return request.Kind() == Data::DataKind::Elevation && request.Range() && cell && ValidCell(*cell)
             ? Data::Coverage::Inside
             : Data::Coverage::Outside;
}

std::string CopernicusDemSource::Url(const Data::Address &at) const {
  const auto cell = at.Cell();
  if (!cell || !ValidCell(*cell)) { return {}; }
  const std::string name = "Copernicus_DSM_COG_10_" +
                           Coordinate(cell->SouthDeg, "NS", kLatitudeDigits) + "_00_" +
                           Coordinate(cell->WestDeg, "EW", kLongitudeDigits) + "_00_DEM";
  return Decl_.Endpoint + name + '/' + name + ".tif";
}

Data::FetchStart CopernicusDemSource::Begin([[maybe_unused]] const Data::Address &at,
                                            [[maybe_unused]] Data::Transport &transport) const {
  return std::unexpected(Data::SourceFailureReason::InvalidRequest);
}

Data::FetchStart CopernicusDemSource::Begin(const Data::SourceRequest &request,
                                            Data::Transport &transport) const {
  const auto &range = request.Range();
  if (!range || Covers(request) != Data::Coverage::Inside || !range->Valid() ||
      (!request.EntityTag().empty() && !Data::StrongEntityTag(request.EntityTag()))) {
    return std::unexpected(Data::SourceFailureReason::InvalidRequest);
  }
  if (range->Length > Decl_.MaximumPayloadBytes) {
    return std::unexpected(Data::SourceFailureReason::CapacityRefused);
  }
  return transport.Begin(Url(request.Where()), *range, request.EntityTag());
}

Data::SourceResponse CopernicusDemSource::Collect([[maybe_unused]] const Data::Address &at,
                                                  Data::Ticket ticket,
                                                  Data::Transport &transport) const {
  auto wire = transport.Collect(ticket);
  switch (wire.Where()) {
    case Data::Wire::State::Working: return Data::SourceResponse::Working();
    case Data::Wire::State::Unreachable: {
      const auto reason = wire.FailureReason();
      const bool terminal = reason == Data::SourceFailureReason::Cancelled ||
                            reason == Data::SourceFailureReason::CapacityRefused;
      return Data::SourceResponse::Meant(terminal ? Data::Meaning::Refused : Data::Meaning::Retry,
                                         reason);
    }
    case Data::Wire::State::Consumed:
    case Data::Wire::State::Never:
      return Data::SourceResponse::Meant(Data::Meaning::Refused, wire.FailureReason());
    case Data::Wire::State::Answered: break;
  }
  auto response = wire.Take();
  if (!response) { return Data::SourceResponse::Meant(Data::Meaning::Refused); }
  if (response->Status == kHttpNotFound) { return Data::SourceResponse::NotFound(); }
  if (response->Status == kHttpPreconditionFailed) {
    return Data::SourceResponse::Meant(Data::Meaning::Refused,
                                       Data::SourceFailureReason::SourceChanged);
  }
  if (response->Status == kHttpTimeout || response->Status == kHttpTooMany ||
      response->Status >= kHttpServerFirst) {
    return Data::SourceResponse::MeantAfter(Data::Meaning::Retry,
                                            wire.RetryAfterS(),
                                            response->Status == kHttpTimeout
                                                ? Data::SourceFailureReason::TimedOut
                                                : Data::SourceFailureReason::Unavailable);
  }
  if (response->Status != Data::kHttpPartialContent || !response->Range ||
      !response->Range->Valid(response->Body.size())) {
    return Data::SourceResponse::Meant(Data::Meaning::Refused,
                                       Data::SourceFailureReason::CorruptPayload);
  }
  return Data::SourceResponse::Delivered(std::move(response->Body), std::move(response->Range));
}

}

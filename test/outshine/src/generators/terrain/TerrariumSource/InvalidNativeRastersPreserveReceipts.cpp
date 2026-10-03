#include "TerrainDelivery.h"
#include "Check.h"
#include <world/data/Source.h>
#include <limits>
#include <memory>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Data;
enum class Payload { Valid, OneRow, Incomplete, Nonfinite, Refused };

class Native final : public Source {
public:
  explicit Native(Payload payload) : Payload_(payload) {}

  SourceDecl Decl;
  mutable const float *Allocation = nullptr;

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Meant(Meaning::Refused);
  }

  std::expected<HeightRaster, DecodeFailure>
  DecodeElevation(std::span<const uint8_t>) const override {
    if (Payload_ == Payload::Refused) { return std::unexpected(DecodeFailure::CorruptPayload); }
    HeightRaster raster{.Rows = 2, .Cols = 2, .Meters = {-1.5f, 0.0f, 1.25f, 172.875f}};
    if (Payload_ == Payload::OneRow) {
      raster.Rows = 1;
      raster.Cols = 4;
    }
    if (Payload_ == Payload::Incomplete) { raster.Meters.pop_back(); }
    if (Payload_ == Payload::Nonfinite) {
      raster.Meters[3] = std::numeric_limits<float>::quiet_NaN();
    }
    Allocation = raster.Meters.data();
    return raster;
  }

private:
  Payload Payload_;
};
}

int main() {
  using namespace outshine::Test;
  const TileId at{.Zoom = 3, .X = 2, .Y = 2};
  const Fetch request(DataKind::Elevation, Address::At(at));
  for (const auto payload : {Payload::Valid,
                             Payload::OneRow,
                             Payload::Incomplete,
                             Payload::Nonfinite,
                             Payload::Refused}) {
    Native source(payload);
    auto decoded = Ground::FromTerrainDelivery(request,
                                               {.Bytes = {42, 17},
                                                .SourceId = "actual-source",
                                                .SourceRevision = "actual-revision",
                                                .SourceKey = "actual-key",
                                                .At = Address::At(at)},
                                               &source);
    if (payload == Payload::Valid) {
      auto native = decoded.Take();
      CHECK(native && std::holds_alternative<Ground::TerrainField>(native->Samples),
            "opaque source bytes become only the provider's native height product");
      if (native) {
        const auto &field = std::get<Ground::TerrainField>(native->Samples);
        CHECK(field.Data() == source.Allocation,
              "provider's float allocation transfers without copying");
        CHECK(field.AtM(0, 0) == -1.5f && field.AtM(1, 1) == 172.875f,
              "negative and fractional native samples are unchanged");
      }
    } else {
      CHECK(decoded.Where() == Ground::TerrainBytes::State::Refused && decoded.Failure(),
            "invalid native products prevent world publication");
      if (decoded.Failure()) {
        const auto &failure = *decoded.Failure();
        CHECK(failure.Reason == FetchFailureReason::CorruptPayload &&
                  failure.SourceId == "actual-source" &&
                  failure.SourceRevision == "actual-revision" &&
                  failure.SourceKey == "actual-key" && failure.Requested == request.Where() &&
                  failure.Served == Address::At(at),
              "decode errors retain the actual served receipt and original demand");
      }
    }
  }
  return Report();
}

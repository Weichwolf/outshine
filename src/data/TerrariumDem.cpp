#include "TerrariumDem.h"

#include <cstdio>

namespace outshine::Data {
namespace {

[[nodiscard]] SourceDecl Declared() {
  SourceDecl d;
  d.Id = "terrarium.s3";
  d.Version = 1;
  d.Kind = DataKind::Elevation;
  d.How = Scheme::TileZxy;
  d.Wire = WireFormat::TerrariumPng;
  d.Order = Rank{0};
  d.MinZoom = 0;
  /* The last zoom the bucket holds; z16 measured 404. It is declared here and nowhere else. */
  d.MaxZoom = 15;
  d.AncestorFill = true;
  d.Keeps = Cacheability::Forever;
  d.Need = Necessity::Required;
  d.Latency = LatencyClass::Distant;
  /* Measured 2026-08-12 over five addresses: 757 B mid-ocean to 105 kB over the Weserbergland. The
   * declared figure is the working middle a budget is planned against, and the telemetry row carries
   * the measured mean beside it so a drift is visible rather than assumed. */
  d.TypicalPayloadBytes = 60000;
  d.RetryBudget = 4;
  return d;
}

} // namespace

TerrariumDem::TerrariumDem() : WebTileSource(Declared()) {}

std::string TerrariumDem::Url(const Address &at) const {
  int z = 0;
  uint32_t x = 0, y = 0;
  if (!at.TryTile(&z, &x, &y)) return std::string();
  char url[160];
  std::snprintf(url, sizeof url,
                "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%u/%u.png", z, x, y);
  return std::string(url);
}

Meaning TerrariumDem::Classify(int status, size_t bytes) const noexcept {
  /* A 200 WITH NO BODY IS NOT AN ANSWER: it is a truncated transfer wearing a success code, and the
   * decoder downstream would report it as a malformed PNG under this tile's name. */
  if (status == 200) return bytes > 0 ? Meaning::Bytes : Meaning::Retry;
  /* THE BUCKET HAS NO SUCH OBJECT, and that is the only absence this upstream can express — it has
   * no 204 and never had one. The body of a 404 here is an S3 XML error, so the length says nothing. */
  if (status == 403 || status == 404) return Meaning::Absent;
  if (status == 408 || status == 429 || status >= 500) return Meaning::Retry;
  return Meaning::Refused;
}

} // namespace outshine::Data

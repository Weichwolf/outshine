#include "OccupancySink.h"

#include <cassert>
#include <cmath>

namespace outshine::Generators {

namespace {

constexpr uint32_t kNoBody = 0xffffffffu;

[[nodiscard]] bool CylindersCut(const Body &a, const Body &b) {
  const double de = a.Em - b.Em, dn = a.Nm - b.Nm;
  const double reach = (double)a.RadiusM + (double)b.RadiusM;
  if (de * de + dn * dn >= reach * reach) { return false; }
  return a.BaseAslM < b.BaseAslM + (double)b.HeightM && b.BaseAslM < a.BaseAslM + (double)a.HeightM;
}

}

OccupancySink::OccupancySink(const Storage &storage) : Store_(storage) {
  assert(Store_.Links.Size() == Store_.Bodies.Size());
  assert(Store_.CellM > 0.0);
}

int OccupancySink::Cells(double spanM, double cellM) {
  const int n = (int)std::ceil(spanM / cellM);
  return n < 1 ? 1 : n;
}

void OccupancySink::Open(const Ground &ground) noexcept {
  SpanEm_ = ground.Where().SpanEm();
  SpanNm_ = ground.Where().SpanNm();
  CellsE_ = Cells(SpanEm_, Store_.CellM);
  CellsN_ = Cells(SpanNm_, Store_.CellM);
  assert((size_t)CellsE_ * (size_t)CellsN_ <= Store_.Cells.Size());
  for (size_t i = 0, cells = (size_t)CellsE_ * (size_t)CellsN_; i < cells; i++) {
    Store_.Cells[i] = kNoBody;
  }
  for (uint32_t &claims : Claims_) { claims = 0; }
  MaxRadiusM_ = 0.0f;
}

int OccupancySink::CellOf(double m, int cells) const noexcept {
  const int i = (int)std::floor(m / Store_.CellM);
  return i < 0 ? 0 : (i >= cells ? cells - 1 : i);
}

bool OccupancySink::Clear(const Body &body) const noexcept {
  const double reach = (double)body.RadiusM + (double)MaxRadiusM_;
  const int e0 = CellOf(body.Em - reach, CellsE_), e1 = CellOf(body.Em + reach, CellsE_);
  const int n0 = CellOf(body.Nm - reach, CellsN_), n1 = CellOf(body.Nm + reach, CellsN_);
  for (int n = n0; n <= n1; n++) {
    for (int e = e0; e <= e1; e++) {
      for (uint32_t i = Store_.Cells[(size_t)n * (size_t)CellsE_ + (size_t)e]; i != kNoBody;
           i = Store_.Links[i]) {
        if (CylindersCut(body, Store_.Bodies[i])) { return false; }
      }
    }
  }
  return true;
}

Claim OccupancySink::Place(const Body &body) noexcept {
  const auto refuse = [this](Claim::Outcome why) {
    Claims_[(size_t)why]++;
    return Claim::Refused(why);
  };
  if (Count() >= Store_.Bodies.Size()) { return refuse(Claim::Outcome::Full); }
  if (!(body.Em >= 0.0) || !(body.Nm >= 0.0) || !(body.Em < SpanEm_) || !(body.Nm < SpanNm_)) {
    return refuse(Claim::Outcome::Outside);
  }
  if (!Clear(body)) { return refuse(Claim::Outcome::Occupied); }

  const uint32_t slot = Count()++;
  Store_.Bodies[slot] = body;
  Store_.Bodies[slot].Id_ = slot;
  const size_t cell =
      (size_t)CellOf(body.Nm, CellsN_) * (size_t)CellsE_ + (size_t)CellOf(body.Em, CellsE_);
  Store_.Links[slot] = Store_.Cells[cell];
  Store_.Cells[cell] = slot;
  if (body.RadiusM > MaxRadiusM_) { MaxRadiusM_ = body.RadiusM; }
  return Claim::Of(BodyId(slot));
}

}

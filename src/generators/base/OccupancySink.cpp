#include "OccupancySink.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace outshine::Generators {

constexpr uint32_t kNoCell = 0xffffffffu;

namespace {

constexpr uint32_t kNoBody = kNoCell;

[[nodiscard]] bool CylindersCut(const Solid &a, const Solid &b) {
  const double de = a.Em - b.Em;
  const double dn = a.Nm - b.Nm;
  const double reach = static_cast<double>(a.RadiusM) + static_cast<double>(b.RadiusM);
  if (de * de + dn * dn >= reach * reach) { return false; }
  return a.BaseAslM < b.BaseAslM + static_cast<double>(b.HeightM) &&
         b.BaseAslM < a.BaseAslM + static_cast<double>(a.HeightM);
}

} // namespace

OccupancySink::OccupancySink(const Storage &storage) : Store_(storage) {
  assert(Store_.Links.size() == Store_.Bodies.size());
  assert(Store_.CellM > 0.0);
}

int OccupancySink::Cells(double spanM, double cellM) {
  const int n = static_cast<int>(std::ceil(spanM / cellM));
  return n < 1 ? 1 : n;
}

void OccupancySink::Open(const Ground &ground) noexcept {
  SpanEm_ = ground.Where().SpanEm();
  SpanNm_ = ground.Where().SpanNm();
  CellsE_ = Cells(SpanEm_, Store_.CellM);
  CellsN_ = Cells(SpanNm_, Store_.CellM);
  assert((size_t)CellsE_ * (size_t)CellsN_ <= Store_.Cells.size());
  for (size_t i = 0, cells = static_cast<size_t>(CellsE_) * static_cast<size_t>(CellsN_); i < cells;
       i++) {
    Store_.Cells[i] = kNoBody;
  }
  for (uint32_t &claims : Claims_) { claims = 0; }
  MaxRadiusM_ = 0.0f;
}

int OccupancySink::CellOf(double m, Axis on) const noexcept {
  const int cells = on == Axis::East ? CellsE_ : CellsN_;
  const int i = static_cast<int>(std::floor(m / Store_.CellM));
  return std::clamp(i, 0, cells - 1);
}

bool OccupancySink::Clear(const Solid &body) const noexcept {
  const double reach = static_cast<double>(body.RadiusM) + static_cast<double>(MaxRadiusM_);
  const int e0 = CellOf(body.Em - reach, Axis::East);
  const int e1 = CellOf(body.Em + reach, Axis::East);
  const int n0 = CellOf(body.Nm - reach, Axis::North);
  const int n1 = CellOf(body.Nm + reach, Axis::North);
  for (int n = n0; n <= n1; n++) {
    for (int e = e0; e <= e1; e++) {
      for (uint32_t i = Store_.Cells[static_cast<size_t>(n) * static_cast<size_t>(CellsE_) +
                                     static_cast<size_t>(e)];
           i != kNoBody;
           i = Store_.Links[i]) {
        if (CylindersCut(body, Store_.Bodies[i])) { return false; }
      }
    }
  }
  return true;
}

Claim OccupancySink::Place(const Solid &body) noexcept {
  const auto refuse = [this](Claim::Outcome why) {
    Claims_[static_cast<size_t>(why)]++;
    return Claim::Refused(why);
  };
  if (Count() >= Store_.Bodies.size()) { return refuse(Claim::Outcome::Full); }
  if (!(body.Em >= 0.0) || !(body.Nm >= 0.0) || !(body.Em < SpanEm_) || !(body.Nm < SpanNm_)) {
    return refuse(Claim::Outcome::Outside);
  }
  if (!Clear(body)) { return refuse(Claim::Outcome::Occupied); }

  const uint32_t slot = Count()++;
  Store_.Bodies[slot] = body;
  const size_t cell =
      static_cast<size_t>(CellOf(body.Nm, Axis::North)) * static_cast<size_t>(CellsE_) +
      static_cast<size_t>(CellOf(body.Em, Axis::East));
  Store_.Links[slot] = Store_.Cells[cell];
  Store_.Cells[cell] = slot;
  MaxRadiusM_ = std::max(body.RadiusM, MaxRadiusM_);
  return Claim::Placed();
}

} // namespace outshine::Generators

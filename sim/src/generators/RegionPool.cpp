#include "RegionPool.h"

namespace outshine::Generators {

RegionPool::RegionPool(const Schedule &schedule, const Shape &shape) {
  /* The equatorial region of the schedule's zoom is the widest and the tallest one it can name:
   * east shrinks with cos(latitude) and the Mercator row shrinks with it. */
  const Region widest = Region::Of(schedule.Zoom(), 0.0, 0.0);
  const size_t cells = (size_t)OccupancySink::Cells(widest.SpanEm(), shape.CellM) *
                       (size_t)OccupancySink::Cells(widest.SpanNm(), shape.CellM);
  const int sinks = shape.Sinks < 1 ? 1 : shape.Sinks;
  Slots_.resize((size_t)sinks);
  for (Slot &s : Slots_) {
    s.Bodies.resize(shape.BodyCapacity);
    s.Links.resize(shape.BodyCapacity);
    s.Cells.resize(cells);
    OccupancySink::Storage storage;
    storage.Bodies = Span<Body>(s.Bodies.data(), s.Bodies.size());
    storage.Links = Span<uint32_t>(s.Links.data(), s.Links.size());
    storage.Cells = Span<uint32_t>(s.Cells.data(), s.Cells.size());
    storage.CellM = shape.CellM;
    s.Sink = std::make_unique<OccupancySink>(storage);
    Bytes_ += s.Bodies.size() * sizeof(Body) + (s.Links.size() + s.Cells.size()) * sizeof(uint32_t);
  }
}

std::optional<RegionPool::Lease> RegionPool::TryAcquire(const Ground &ground) {
  for (size_t i = 0; i < Slots_.size(); i++) {
    if (Slots_[i].Out) continue;
    Slots_[i].Out = true;
    Slots_[i].Sink->Open(ground);
    return Lease(*this, i);
  }
  return std::nullopt;
}

size_t RegionPool::Free() const {
  size_t n = 0;
  for (const Slot &s : Slots_)
    if (!s.Out) n++;
  return n;
}

void RegionPool::Release(size_t slot) { Slots_[slot].Out = false; }

RegionPool::Lease::Lease(RegionPool &pool, size_t slot)
    : Pool_(&pool), Slot_(slot), Sink_(pool.Slots_[slot].Sink.get()) {}

RegionPool::Lease::Lease(Lease &&other) noexcept
    : Pool_(other.Pool_), Slot_(other.Slot_), Sink_(other.Sink_) {
  other.Pool_ = nullptr;
}

RegionPool::Lease &RegionPool::Lease::operator=(Lease &&other) noexcept {
  if (this == &other) return *this;
  if (Pool_) Pool_->Release(Slot_);
  Pool_ = other.Pool_;
  Slot_ = other.Slot_;
  Sink_ = other.Sink_;
  other.Pool_ = nullptr;
  return *this;
}

RegionPool::Lease::~Lease() {
  if (Pool_) Pool_->Release(Slot_);
}

} // namespace outshine::Generators

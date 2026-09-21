#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"

#include <memory>

namespace {

class NoTransport final : public outshine::Data::Transport {
public:
  outshine::Data::Ticket Begin(const std::string &) override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel(outshine::Data::Ticket) override {}
};

}

int main() {
  using namespace outshine::Test;
  using outshine::Data::ContentStore;
  using outshine::Data::SourceSet;
  using outshine::Ground::TerrainField;
  using outshine::Ground::TilePool;

  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  NoTransport transport;
  TilePool pool({}, sources, transport);
  pool.Shapes({.Kind = "sineRidge", .AmplitudeM = 4.0, .WavelengthM = 100.0});
  const size_t empty = pool.SchedulerBytes();
  std::shared_ptr<const TerrainField> field;
  CHECK(pool.Field({.Zoom = 12, .X = 1200, .Y = 1500}, &field) == TilePool::Reply::Pending,
        "field request enters scheduler");
  for (int attempt = 0; attempt < 4 && pool.Counters().Held == 0; ++attempt) {
    (void)pool.AwaitLanding(5.0);
  }
  CHECK(pool.Counters().Held == 1, "completed field remains held until the caller takes it");
  const size_t completed = pool.SchedulerBytes();
  constexpr size_t side = 257;
  constexpr size_t fieldBytes = side * side * sizeof(float);
  CHECK(completed >= empty + fieldBytes,
        "completed field capacity remains in scheduler residency until the caller takes it");
  const TilePool::Reply received = pool.Field({.Zoom = 12, .X = 1200, .Y = 1500}, &field);
  CHECK(received == TilePool::Reply::Ready, "caller takes a ready retained native field");
  CHECK(field && field->Rows() == side && field->Cols() == side,
        "taken native field has the generated terrain resolution");
  CHECK(pool.SchedulerBytes() < completed,
        "taking a consumed field releases its scheduler-owned payload from the budget");
  return Report();
}

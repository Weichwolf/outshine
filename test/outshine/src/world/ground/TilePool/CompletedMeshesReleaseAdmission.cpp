#include "TilePool.h"
#include "SourceSet.h"
#include "Check.h"

namespace {
class NoTransport final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin(const std::string &) override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel(outshine::Data::Ticket) override {}
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  NoTransport transport;
  Ground::TilePool pool({.OutstandingMost = 1}, sources, transport);
  pool.Shapes({.Kind = "sineRidge", .AmplitudeM = 4.0, .WavelengthM = 100.0});
  TileBuild first;
  const Data::TileId original{.Zoom = 12, .X = 1200, .Y = 1500};
  CHECK(pool.MeshAwaited(original, 64, &first) == TileMeshes::Reply::Ready,
        "first mesh completes and remains cached");
  CHECK(pool.Counters().Outstanding == 0 && pool.Counters().Held == 1,
        "retained completion is not outstanding work");
  TileBuild second;
  CHECK(pool.MeshAwaited({.Zoom = 12, .X = 1201, .Y = 1500}, 64, &second) ==
            TileMeshes::Reply::Ready,
        "a retained result does not block the next distinct mesh at capacity one");
  TileBuild repeated;
  CHECK(pool.Mesh(original, 64, &repeated) == TileMeshes::Reply::Ready && !first.Nodes.empty() &&
            repeated.Nodes == first.Nodes,
        "new admission preserves the earlier cached mesh");
  CHECK(pool.Counters().Posts == 2 && pool.Counters().Held == 2,
        "completed-result retention and outstanding admission have separate bounds");
  CHECK(!pool.AwaitLanding(0.001),
        "already consumed cached meshes cannot signal new landing progress");
  const Data::TileId pending{.Zoom = 12, .X = 1202, .Y = 1500};
  TileBuild third;
  CHECK(pool.Mesh(pending, 64, &third) == TileMeshes::Reply::Pending,
        "a new mesh is admitted beside retained results");
  CHECK(pool.AwaitLanding(5.0), "a new completion wakes the waiter");
  CHECK(pool.AwaitLanding(0.001), "waiting alone does not consume a pending result");
  pool.ForgetMesh(pending.Zoom, pending.X, pending.Y);
  CHECK(!pool.AwaitLanding(0.001), "forgetting an unclaimed result removes its wake condition");
  CHECK(pool.MeshAwaited(pending, 64, &third) == TileMeshes::Reply::Ready,
        "forgotten mesh can be generated and consumed again");
  CHECK(!pool.AwaitLanding(0.001), "taking the replacement clears its wake condition");
  return Report();
}

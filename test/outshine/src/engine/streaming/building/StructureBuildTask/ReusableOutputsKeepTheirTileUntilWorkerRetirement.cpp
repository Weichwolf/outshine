#include "RetirementFixture.h"
#include "BuildingGeometry.h"
#include "Check.h"
#include <atomic>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {
struct Coordinates {
  std::thread::id Frame;
  std::atomic<bool> *FreedOnFrame = nullptr;
  std::vector<double> Points{47, 9, 47, 9.0001, 47.0001, 9.0001, 47.0001, 9};

  ~Coordinates() { FreedOnFrame->store(std::this_thread::get_id() == Frame); }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::Test::Retirement;
  Tasks pool(1);
  const JobAdmission admission(1);
  const auto control = std::make_shared<Control>();
  control->Frame = std::this_thread::get_id();
  std::atomic<bool> coordinatesFreedOnFrame{false};
  std::weak_ptr<Coordinates> retained;
  auto task = Task(admission, control);
  {
    const auto coordinates = std::make_shared<Coordinates>();
    coordinates->Frame = control->Frame;
    coordinates->FreedOnFrame = &coordinatesFreedOnFrame;
    retained = coordinates;
    auto geometry = std::make_shared<Ground::BuildingGeometry>();
    geometry->ShareCoordinates(
        coordinates, coordinates->Points, {}, coordinates->Points.capacity() * sizeof(double));
    task.Result().Tile.emplace().Coordinates = std::move(geometry);
  }
  const auto output = task.TakeOutputForReuse();
  CHECK(output && !output->Tile && !retained.expired(),
        "an empty reusable output retains its previous geometry owner in the retired payload");
  CHECK(task.Retire(pool), "the old baked tile is handed to the existing worker");
  Drain(pool);
  CHECK(retained.expired() && !coordinatesFreedOnFrame.load(),
        "the old tile releases its last actual coordinate owner outside the frame");
  Released(admission, *control);
  return Report();
}

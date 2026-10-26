#include "Views.h"
#include "Check.h"

#include <array>
#include <limits>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::View original;
  original.Id = "moving";
  original.Placement = Scenario::CameraPlacement::Geodetic;
  original.Geographic.SamplesHeight = true;
  original.Follows = "previous-body";
  original.Person = "third";
  original.TimeScale = 0.5;
  original.OffsetM = {{4, 5, 6}};
  original.Sees.FovDeg = 73;
  original.Sees.NearM = 0.25;
  original.Sees.FarM = 1500;
  auto book = ViewBook::Stand(std::array{original}, original.Id);
  CHECK(book.has_value(), "the original terrain-following view is valid");
  if (!book) { return Report(); }
  const auto *const owned = &book->Active();
  Camera pose;
  pose.PositionM = {{7, 8, 9}};
  pose.LooksAt = true;
  pose.LookAtM = {{8, 8, 9}};
  pose.UpM = {{0, 1, 0}};
  pose.FovDeg = 5;
  static_assert(noexcept(book->SetCameraPose(pose)));
  CHECK(book->SetCameraPose(pose).has_value(), "a finite right-facing pose is accepted");
  const auto &view = book->Active();
  CHECK(&view == owned && book->Count() == 1 && view.Id == original.Id,
        "pose changes preserve the existing owned entry and its identity");
  CHECK(view.Placement == Scenario::CameraPlacement::Local && view.Follows.empty() &&
            view.Person.empty() && !view.Geographic.SamplesHeight,
        "free camera input releases follow, route and sampled-height control");
  CHECK(view.OffsetM == Vec3{} && view.Sees.PositionM == pose.PositionM,
        "an old displacement is not added to the supplied absolute local position");
  CHECK(view.Sees.FovDeg == original.Sees.FovDeg && view.Sees.NearM == original.Sees.NearM &&
            view.Sees.FarM == original.Sees.FarM && view.TimeScale == original.TimeScale,
        "projection and the simulation clock remain unchanged");
  Mat4 model;
  CHECK(view.Sees.modelMatrix(model).has_value() && model[8] == -1 && model[9] == 0 &&
            model[10] == 0 && model[12] == 7 && model[13] == 8 && model[14] == 9,
        "the accepted pose actually points east from the requested position");
  const auto reject = [&](Camera invalid) {
    CHECK(!book->SetCameraPose(invalid), "invalid camera input is rejected");
    Mat4 retained;
    CHECK(book->Active().Sees.modelMatrix(retained).has_value() && retained == model &&
              book->Active().Id == original.Id,
          "failure preserves the complete previous pose and identity");
  };
  auto invalid = pose;
  invalid.PositionM[0] = std::numeric_limits<double>::quiet_NaN();
  reject(invalid);
  invalid = pose;
  invalid.LookAtM = invalid.PositionM;
  reject(invalid);
  invalid = pose;
  invalid.UpM = {{1, 0, 0}};
  reject(invalid);
  auto moved = std::move(*book);
  CHECK(!book->SetCameraPose(pose), "a moved-from book does not access an absent entry");
  CHECK(moved.SetCameraPose(pose).has_value(), "the new owner retains the active view");
  return Report();
}

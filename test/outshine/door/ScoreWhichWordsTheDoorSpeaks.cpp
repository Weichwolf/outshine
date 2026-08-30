#include <cstdio>
#include <type_traits>
#include <vector>

#include "Check.h"
#include "Geometry.h"
#include "Material.h"
#include "Outshine.h"
#include "Scenario.h"

// WHICH WORDS THE DOOR SPEAKS, AND WHICH ONES IT REFUSES TO SPEAK TWICE.
//
// The engine's whole claim about `include/` is that a reader arrives already owning the vocabulary:
// Filament's for the renderer, Cesium's for the Earth. That claim had NO case. It was checked by
// grepping the header and counting names, which is a measure that cannot see the two things that
// actually matter -- whether a name is REACHABLE from outside, and whether the old spelling is
// still standing beside the new one. A door with two ways to draw a frame has not been renamed; it
// has grown a synonym, and a synonym is the shape a half-finished rename leaves behind.
//
// So the claims here are about ACCESS rather than about spelling, because access is the half a
// grep is blind to:
//
//   SPOKEN     each name resolves at the door with the shape a Filament or Cesium reader owns.
//              Most of this is the compiler's -- a missing name does not link -- so what the case
//              adds is the detector that the other two claims need.
//   ONE DOOR   `Engine::render` is NOT reachable from a client and `Engine::renderer` is. Both
//              halves are asserted: a detector that always answered "unreachable" would prove the
//              first for free, and the second is what stops it.
//   TOLD APART a part index cannot be passed where a `outshine::MaterialInstance` belongs. Both
//   were `int`
//              until this round and the compiler could not see the difference; the control is that
//              the deliberate spelling still compiles.
//
// And `TransformManager` gets a behaviour claim rather than a naming one, because it was written
// long ago with NO caller anywhere in the tree -- an unreachable capability is also an unmeasured
// one, so naming it without exercising it would move the defect rather than close it.
//
// WHAT THIS CASE DOES NOT COVER: whether the names are the RIGHT ones. That is a judgement against
// Filament's and Cesium's own headers and no case can hold it. It also says nothing about whether
// a client is SHORT -- line counts live in the item, not here, because a rate has no negative
// control. And it does not reach `Scene`, `View` or `Camera` behaviour: those have their own cases.

namespace {

using namespace outshine;

template <typename E>
concept DrawsThroughEngine = requires(E &e, Extent frame) { e.render(frame); };

template <typename E>
concept HandsOverARenderer = requires(E &e) { e.renderer(); };

template <typename G>
concept TakesABareIntAsMaterial = requires(G &g) { g.addPart("part", 3); };

template <typename G>
concept TakesAMaterialInstance =
    requires(G &g, outshine::MaterialInstance bound) { g.addPart("part", bound); };

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // SPOKEN. Twelve words, and the ones that carry data are pinned by shape rather than by name
  // alone: a `LongitudeLatitudeHeight` whose members were renamed would still be spelled right.
  static_assert(std::is_same_v<decltype(LongitudeLatitudeHeight::LongitudeDeg), double>);
  static_assert(std::is_same_v<decltype(LongitudeLatitudeHeight::LatitudeDeg), double>);
  static_assert(std::is_same_v<decltype(LongitudeLatitudeHeight::HeightM), double>);
  static_assert(std::is_same_v<decltype(Georeference::LatitudeDeg), double>);
  static_assert(std::is_same_v<decltype(Standing::GlobeAnchor), bool>);
  static_assert(std::is_same_v<decltype(Camera::FovDeg), double>);
  static_assert(std::is_same_v<decltype(View::Id), std::string>);
  static_assert(std::is_default_constructible_v<Material>);
  static_assert(std::is_default_constructible_v<outshine::MaterialInstance>);
  static_assert(!std::is_default_constructible_v<Renderer>);
  static_assert(!std::is_default_constructible_v<TransformManager>);
  std::printf("  twelve door words resolve, and the five that carry numbers by SHAPE\n");

  // ONE DOOR. The second assertion is the control: without it a detector that answered "no" to
  // everything would prove the first for nothing.
  std::printf("  Engine::render reachable from a client: %s   Engine::renderer: %s\n",
              DrawsThroughEngine<Engine> ? "YES" : "no",
              HandsOverARenderer<Engine> ? "YES" : "no");
  CHECK(HandsOverARenderer<Engine>,
        "**THE DETECTOR CAN SEE A REACHABLE NAME**: this is the control for the claim below. If it "
        "goes red the next assertion proves nothing, because a detector that answers no to every "
        "name would satisfy it for free");
  CHECK(!DrawsThroughEngine<Engine>,
        "**THERE IS ONE WAY TO DRAW A FRAME**: `Engine::render` standing beside "
        "`Engine::renderer().render` is not a renamed door, it is a door with a synonym -- and a "
        "synonym is what a half-finished rename leaves behind. A reader who owns Filament looks "
        "for the Renderer and must not find a second answer next to it");

  // TOLD APART. Both were `int` until this round, so `Part(name, someOtherIndex)` compiled and
  // meant something wrong. The control is the second assertion.
  std::printf("  Geometry::addPart takes a bare int: %s   a outshine::MaterialInstance: %s\n",
              TakesABareIntAsMaterial<Geometry> ? "YES" : "no",
              TakesAMaterialInstance<Geometry> ? "YES" : "no");
  CHECK(TakesAMaterialInstance<Geometry>,
        "**THE DETECTOR CAN SEE THE DELIBERATE SPELLING**: control for the claim below");
  CHECK(!TakesABareIntAsMaterial<Geometry>,
        "**A PART INDEX IS NOT A MATERIAL**: while both were `int` the compiler could not tell a "
        "part handle from a surface handle, and passing one for the other is a defect that draws "
        "-- wrongly -- rather than one that fails. `outshine::MaterialInstance` is what makes it a "
        "build "
        "error, which is stricter than any case can be");

  // TRANSFORMMANAGER. It had no caller in the whole tree, so this is the first time the capability
  // is exercised at all. The refusal is the negative control and it is what the old `void Place`
  // could not give: it returned silently on a part that does not exist.
  Geometry made;
  const outshine::MaterialInstance surface = made.addSurface("plain", Material{});
  const int part = made.addPart("one", surface);
  const double shifted[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 12.5, -3.25, 7.0, 1};
  TransformManager transforms = made.transforms();
  const bool set = transforms.setTransform(part, shifted);
  const double *read = transforms.getTransform(part);
  const bool same = read != nullptr && read[12] == 12.5 && read[13] == -3.25 && read[14] == 7.0;
  const bool refused = !transforms.setTransform(-1, shifted);
  const bool alsoRefused = !transforms.setTransform(made.parts(), shifted);
  std::printf("  transform set %s, read back %s, part -1 %s, part %d %s\n",
              set ? "yes" : "NO",
              same ? "yes" : "NO",
              refused ? "refused" : "TOOK",
              made.parts(),
              alsoRefused ? "refused" : "TOOK");
  CHECK(set && same,
        "**A TRANSFORM SET THROUGH THE MANAGER IS THE TRANSFORM READ BACK**: the capability has "
        "stood in this tree with no caller, which means it has never been measured either. Naming "
        "it without exercising it would move the defect rather than close it");
  CHECK(refused && alsoRefused,
        "**A PART THAT DOES NOT EXIST IS REFUSED LOUDLY**: `Geometry::Place` returned void and did "
        "nothing, so a caller with a stale handle got a transform that silently never landed. That "
        "is the failure mode this door exists to end -- and it is the negative control for the "
        "claim above, which would pass on any implementation at all if nothing were ever refused");

  // A REFUSAL CARRIES ITS REASON, and carries it ON the return value rather than beside it. The
  // door used to answer `bool` and keep the reason in `Engine::error()`, which is last-write-wins:
  // two refusals in a row and the first reason is gone. `std::expected<void, std::string>` is
  // CLAUDE.md's own rule and it is stricter than Filament's convention, which reports nothing.
  //
  // The measurement is dynamic on purpose. Counting `return std::unexpected(S_->Error)` sites whose
  // method never assigns `S_->Error` reads 3 of 65 -- and all three are wrong, because the reason
  // was set by a CALLEE the static walk cannot see. That is this tree's named "a measure that
  // cannot see" trap, so the reasons are DRIVEN here instead of counted.
  Engine door;
  const Result tooEarly = door.assemble();
  const Result noSuchView = door.setView("a view no scenario declares");
  const Result noSuchFile = door.readScenario("/does/not/exist/at/all.json");
  const Result noSuchSave = door.restore("/does/not/exist/at/all.save");
  const Result noSuchPark = door.resume("nothing was ever parked under this name");
  const Result refusals[] = {tooEarly, noSuchView, noSuchFile, noSuchSave, noSuchPark};
  size_t silent = 0;
  for (const Result &one : refusals) {
    if (one.has_value() || one.error().empty()) { ++silent; }
  }
  std::printf("  five refusals driven, %zu of them silent; first reason: %.60s\n",
              silent,
              tooEarly.has_value() ? "(ACCEPTED)" : tooEarly.error().c_str());
  CHECK(
      silent == 0,
      "**A REFUSAL CARRIES ITS REASON ON THE RETURN VALUE**: `Engine::error()` beside a `bool` is "
      "last-write-wins, so a client that makes two calls before looking gets one reason for two "
      "failures and no way to tell which. A door that cannot say WHY is the defect this session "
      "fixed four times by hand");

  const Result accepted = door.declare(Scenario{});
  std::printf("  an accepted call: %s\n", accepted ? "no error carried" : accepted.error().c_str());
  CHECK(accepted.has_value(),
        "**THE CONTROL: AN ACCEPTED CALL CARRIES NO REASON**. Without it the claim above passes on "
        "an engine that refuses EVERYTHING with a stock string, which is not a door that explains "
        "itself -- it is a door that is shut");

  Covers("board:2016 -- the door speaks Filament and Cesium, and speaks each word ONCE");
  return Report();
}

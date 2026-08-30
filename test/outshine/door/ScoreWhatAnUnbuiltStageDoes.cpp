#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "RenderCatalogue.h"
#include "Compiled.h"
#include "SceneRenderer.h"

namespace {

// A ROW A CONSUMER CAN SELECT AND NOTHING CAN RUN. The catalogue is the PLAN's vocabulary and it
// is allowed to run ahead of the device: a row may declare its resource edges before an executor
// exists. What that costs is a way to find out which rows are still empty, and this case is it.
//
// It does NOT name them. board:1805 counted three by grepping for three names, and board:1990
// deleted those three, which left this case unable to compile -- a case that fails by failing to
// BUILD tells a reader nothing about the tree. So every row here is DERIVED: walk the catalogue,
// keep what `SceneRenderer::Executable` refuses, and the list is whatever it is on the day it runs.
//
// The two possible answers are far apart: a plan naming an unbuilt stage either draws NOTHING and
// says nothing, or refuses by NAME. It refuses -- `SceneRenderer::Init` walks `Plan_->Order()` and
// stops at the first stage `ExecutorOf` cannot seat. So the row is a PROMISE the device checks
// before it draws a frame, and the case pins both halves: a plan of stages the device seats
// compiles and stands, and one naming a row nothing implements is refused with a row's own name.
//
// A derived set that comes back EMPTY makes every check below vacuous, so the case refuses rather
// than passing on nothing -- the day the last row grows an executor, this case says so out loud.
constexpr int kFramePx = 32;

[[nodiscard]] outshine::Render::PlanSpec Naming(outshine::Render::Stage extra, bool with) {
  outshine::Render::PlanSpec out;
  out.Outputs = {outshine::Render::Resource::FrameTex, outshine::Render::Resource::Surface};
  out.Content = {outshine::Render::Stage::Subjects, outshine::Render::Stage::Overlay};
  if (with) { out.Content.push_back(extra); }
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Render;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::vector<Stage> unbuilt;
  for (size_t at = 0; at < kStageCount; ++at) {
    if (!SceneRenderer::Executable((Stage)at)) { unbuilt.push_back((Stage)at); }
  }
  if (unbuilt.empty()) {
    Unprepared(
        "every catalogue row now has an executor, so this case has nothing to measure -- that is "
        "good news and the case is what must be rewritten, not the tree");
    return Report();
  }
  std::vector<Stage> refusedByTheDevice;
  bool everyRowRefuses = true;
  for (const Stage row : unbuilt) {
    const bool seated = SceneRenderer::Executable(row);
    auto made = Compiled::Compile(Naming(row, true));
    if (made) { refusedByTheDevice.push_back(row); }
    std::printf("  %-10s catalogue row, device seats it: %-3s   plan compiles: %s\n",
                Row(row).Name,
                seated ? "YES" : "no",
                made ? "yes" : "no");
    everyRowRefuses = everyRowRefuses && !seated;
  }

  CHECK(everyRowRefuses,
        "every row this case derived is one the device does not seat -- that is how they were "
        "derived, and the check is here so a `Executable` that starts answering YES to its own "
        "input is caught rather than trusted");
  // AN UNBUILT ROW COMES IN TWO KINDS and the old three were all one kind, which is why nobody
  // saw the split. Naming a row adds it to a minimal plan WITHOUT its producers: for most rows
  // the graph still closes and the refusal that follows is the DEVICE's, which is the property
  // this case is about. For a row whose inputs nothing in that plan produces -- `irradiance`
  // reads what `mediumRadiance` writes -- the GRAPH refuses first and the device is never asked.
  // Both are correct refusals at different layers, and the device half needs a row of the first
  // kind, so it is picked from the set that compiled rather than from the front of the list.
  CHECK(!refusedByTheDevice.empty(),
        "at least one unbuilt row lands in a plan the graph accepts, so there is something for "
        "the DEVICE to refuse -- if every unbuilt row were refused by the graph first, this case "
        "could say nothing about the device at all and would be measuring the compiler twice");
  if (refusedByTheDevice.empty()) { return Report(); }
  const Stage anUnbuiltRow = refusedByTheDevice.front();
  std::printf("THE GRAPH ACCEPTS %zu OF %zu, so the device is asked about %s\n",
              refusedByTheDevice.size(),
              unbuilt.size(),
              Row(anUnbuiltRow).Name);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so no device can be asked what it seats");
    return Report();
  }

  SceneRenderer device;
  auto plain = Compiled::Compile(Naming(anUnbuiltRow, false));
  if (!plain) {
    Unprepared(("a plan of seated stages would not compile: " + plain.error()).c_str());
    return Report();
  }
  device.Init(kFramePx, kFramePx, *plain);
  const bool stood = device.DeviceUsable();
  std::printf("A PLAN OF SEATED STAGES   device usable: %s   %s\n",
              stood ? "yes" : "no",
              stood ? "" : device.WhyNot().c_str());
  if (!stood) {
    Unprepared(("the device would not stand a plan it seats entirely: " + device.WhyNot()).c_str());
    return Report();
  }

  auto withUnbuilt = Compiled::Compile(Naming(anUnbuiltRow, true));
  if (!withUnbuilt) {
    Unprepared(("a plan naming " + std::string(Row(anUnbuiltRow).Name) +
                " would not compile: " + withUnbuilt.error())
                   .c_str());
    return Report();
  }
  device.Init(kFramePx, kFramePx, *withUnbuilt);
  const bool refused = !device.DeviceUsable();
  std::printf("A PLAN NAMING %-10s REFUSED: %s\n  %s\n",
              Row(anUnbuiltRow).Name,
              refused ? "yes" : "NO",
              device.WhyNot().c_str());

  CHECK(refused,
        "**A STAGE THE DEVICE CANNOT RUN IS REFUSED BEFORE A FRAME IS DRAWN**: the catalogue "
        "offers more than any one device layer implements, which is what makes a second executor "
        "table possible at all, and a consumer that selects a row nothing seats learns so at "
        "stand-up rather than by looking at an empty picture");
  // WHICH row the refusal names need not be the one asked for, and that is a measurement rather
  // than a disappointment: asking for one row pulls its resource edges into the plan, and those can
  // pull a SECOND unbuilt row in with them. The refusal is the device's first stop, not the row
  // that was asked for, and the check below is written to accept any derived row by name.
  size_t unseated = 0;
  std::string named;
  for (size_t at = 0; at < kStageCount; ++at) {
    const Stage row = (Stage)at;
    if (SceneRenderer::Executable(row)) { continue; }
    ++unseated;
    named += std::string(named.empty() ? "" : " ") + Row(row).Name;
  }
  std::printf("CATALOGUE ROWS NO DEVICE LAYER SEATS  %zu: %s\n", unseated, named.c_str());

  bool namesOne = false;
  for (size_t at = 0; at < kStageCount; ++at) {
    const Stage row = (Stage)at;
    if (SceneRenderer::Executable(row)) { continue; }
    if (device.WhyNot().find(Row(row).Name) != std::string::npos) { namesOne = true; }
  }
  CHECK(namesOne,
        "and the refusal NAMES a row the device cannot seat: a reader is told WHICH stage, not "
        "that something somewhere is unimplemented. It need not be the one the consumer asked "
        "for -- a stage pulls its own reads into the plan, and the first unseated row in the "
        "order is the one that stops it");

  Covers("the render plan: the catalogue offers rows a device layer need not implement, a plan "
         "naming one still compiles because its resource edges are consistent, and the DEVICE "
         "refuses it by name at stand-up rather than drawing an empty frame");
  return Report();
}

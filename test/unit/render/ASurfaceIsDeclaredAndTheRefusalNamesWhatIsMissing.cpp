#include <cstdio>
#include <string>

#include "Check.h"

#include "Renderer.h"

using outshine::Render::Renderer;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1837: four public methods landed on Renderer and nothing under test/ named any of
  // them -- both users were tools/viewer, which the fast gate excludes. Every refusal below is
  // a pure-logic path on a renderer that was never given a device, so the mirror can hold the
  // door without a GPU.
  Renderer standing;

  const auto noWindow = standing.ShowOn(nullptr);
  std::printf("NOTE showing on no window says: '%.80s'\n",
              noWindow ? "" : noWindow.error().c_str());
  CHECK(!noWindow && noWindow.error().find("names none") != std::string::npos,
        "**A SURFACE THAT IS NO SURFACE IS REFUSED BY NAME**: a renderer is shown on a window "
        "and a call that names none says so, rather than claiming nothing and drawing nothing "
        "(board:1837)");

  const auto noExtent = standing.ShowOffscreen(0, 0);
  std::printf("NOTE showing into no extent says: '%.80s'\n",
              noExtent ? "" : noExtent.error().c_str());
  CHECK(!noExtent && noExtent.error().find("positive extent") != std::string::npos,
        "and an extent of nothing is refused with the numbers it was handed");

  const auto negative = standing.ShowOffscreen(-4, 8);
  CHECK(!negative && negative.error().find("-4") != std::string::npos,
        "and a negative extent names itself in the refusal, so the caller reads back what it "
        "declared");

  const auto beforeDevice = standing.ShowOffscreen(64, 64);
  std::printf("NOTE showing before a device says: '%.80s'\n",
              beforeDevice ? "" : beforeDevice.error().c_str());
  CHECK(!beforeDevice && beforeDevice.error().find("no device") != std::string::npos,
        "**AND A SURFACE ASKED FOR BEFORE THE DEVICE STANDS IS A DIFFERENT REFUSAL** -- 'this "
        "extent is impossible' and 'there is nothing to make it on' are two facts, and a door "
        "that answers one sentence for both tells the caller nothing");

  const auto nothingShown = standing.PresentFrame();
  std::printf("NOTE presenting with nothing shown says: '%.80s'\n",
              nothingShown ? "" : nothingShown.error().c_str());
  CHECK(!nothingShown && nothingShown.error().find("no window is being shown") != std::string::npos,
        "**AND PRESENTING WITH NO SURFACE DECLARED IS REFUSED RATHER THAN ANSWERED WITH A "
        "FRAME THAT DREW NOTHING**: Shown{false,0,0} came back for four different facts and "
        "the only caller had no else, so a browser whose swapchain stopped answering simply "
        "spun (board:1836)");

  standing.StopShowing();
  const auto stillNothing = standing.PresentFrame();
  CHECK(!stillNothing,
        "and stopping what was never shown is not an error, while presenting after it still is");

  Covers("III.18 the surface door is proven by a case: a client hands in a window or an extent "
         "and every way that can fail names which one, on a renderer that never saw a device "
         "(board:1836, board:1837)");
  return Report();
}

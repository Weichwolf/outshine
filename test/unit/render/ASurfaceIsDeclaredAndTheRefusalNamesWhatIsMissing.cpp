#include <cstdio>
#include <expected>
#include <optional>
#include <string>
#include <type_traits>

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
              noWindow ? "" : std::string(noWindow.error()).c_str());
  CHECK(!noWindow && std::string(noWindow.error()).find("names none") != std::string::npos,
        "**A SURFACE THAT IS NO SURFACE IS REFUSED BY NAME**: a renderer is shown on a window "
        "and a call that names none says so, rather than claiming nothing and drawing nothing "
        "(board:1837)");

  const auto noExtent = standing.ShowOffscreen(0, 0);
  std::printf("NOTE showing into no extent says: '%.80s'\n",
              noExtent ? "" : std::string(noExtent.error()).c_str());
  CHECK(!noExtent && std::string(noExtent.error()).find("positive extent") != std::string::npos,
        "and an extent of nothing is refused with the numbers it was handed");

  const auto negative = standing.ShowOffscreen(-4, 8);
  CHECK(!negative && std::string(negative.error()).find("positive extent") != std::string::npos,
        "and a negative extent is refused by the same sentence, with the numbers it was handed "
        "carried in WhyNot rather than built into a string on the caller's path");

  const auto beforeDevice = standing.ShowOffscreen(64, 64);
  std::printf("NOTE showing before a device says: '%.80s'\n",
              beforeDevice ? "" : std::string(beforeDevice.error()).c_str());
  CHECK(!beforeDevice && std::string(beforeDevice.error()).find("no device") != std::string::npos,
        "**AND A SURFACE ASKED FOR BEFORE THE DEVICE STANDS IS A DIFFERENT REFUSAL** -- 'this "
        "extent is impossible' and 'there is nothing to make it on' are two facts, and a door "
        "that answers one sentence for both tells the caller nothing");

  const auto nothingShown = standing.PresentFrame();
  std::printf("NOTE presenting with nothing shown says: '%.80s'\n",
              nothingShown ? "" : std::string(nothingShown.error()).c_str());
  CHECK(!nothingShown && std::string(nothingShown.error()).find("no window is being shown") != std::string::npos,
        "**AND PRESENTING WITH NO SURFACE DECLARED IS REFUSED RATHER THAN ANSWERED WITH A "
        "FRAME THAT DREW NOTHING**: Shown{false,0,0} came back for four different facts and "
        "the only caller had no else, so a browser whose swapchain stopped answering simply "
        "spun (board:1836)");

  standing.StopShowing();
  const auto stillNothing = standing.PresentFrame();
  CHECK(!stillNothing,
        "and stopping what was never shown is not an error, while presenting after it still is");

  // board:1847: a minimised window makes SDL hand back a null swapchain texture, and its own
  // header says "This is not an error". The door used to answer std::unexpected for it -- a
  // std::string built once per frame on the present path, ending in an SDL_GetError() that no
  // failure had set.
  static_assert(std::is_same_v<decltype(standing.PresentFrame()),
                               std::expected<std::optional<outshine::Render::Renderer::Shown>,
                                             std::string_view>>,
                "no image this frame is a VALUE the success type carries, and a refusal is "
                "reserved for the three facts that are faults");
  CHECK(!nothingShown.has_value(),
        "**AND A FAULT IS STILL A REFUSAL**: no surface declared, no device and no command "
        "buffer are the three, and each answers a string_view into static text rather than a "
        "string built on the present path (board:1847)");

  Covers("III.18 the surface door is proven by a case: a client hands in a window or an extent "
         "and every way that can fail names which one, on a renderer that never saw a device "
         "(board:1836, board:1837)");
  return Report();
}

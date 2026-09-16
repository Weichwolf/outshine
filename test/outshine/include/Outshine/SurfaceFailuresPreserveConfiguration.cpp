#include <Outshine.h>
#include "Check.h"
#include <cassert>
#include <dlfcn.h>
#include <string>
#include <vector>

namespace {
bool rejectSubmit = false;
unsigned rejected = 0;

class Receiver final : public outshine::Host {
public:
  std::string Last;

  bool calls(std::string_view name, std::span<const outshine::Argument>) override {
    Last = name;
    return true;
  }
};
}

extern "C" bool SDLCALL SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer *commands) {
  if (rejectSubmit) {
    rejectSubmit = false;
    ++rejected;
    const bool cancelled = SDL_CancelGPUCommandBuffer(commands);
    assert(cancelled);
    SDL_SetError("injected UI submission failure");
    return false;
  }
  static const auto original = reinterpret_cast<decltype(&SDL_SubmitGPUCommandBuffer)>(
      dlsym(RTLD_NEXT, "SDL_SubmitGPUCommandBuffer"));
  assert(original != nullptr);
  return original(commands);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initializes");
  {
    Receiver receiver;
    Engine engine;
    engine.offers(&receiver);
    (void)engine.setRoots({.Shipped = "src/assets"});
    CHECK(engine.drawsInto(Extent{64, 64}).has_value(), "offscreen target ready");
    Scenario::Document document;
    document.Render.Declared = true;
    document.Render.Outputs = {"surface"};
    Scenario::View view;
    view.Id = "ui-camera";
    view.Person = "first";
    view.Placement = Scenario::CameraPlacement::Local;
    view.Sees.PositionM = {{0, 0, 2}};
    view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
    document.Views.push_back(view);
    document.Surfaces.push_back({.Document = "<div data-action=\"old()\"></div>",
                                 .Style = "div {width:64px;height:64px;background:red;}"});
    const bool ready = engine.declare(document) && engine.assemble() && engine.advance();
    CHECK(ready, "surface and explicit camera ready");
    if (ready) {
      const auto read = [&] {
        std::vector<uint8_t> pixels;
        CHECK(engine.renderer().render({}).has_value(), "UI renders");
        CHECK(engine.renderer().readPixels(pixels).has_value(), "UI pixels readable");
        return pixels;
      };
      const auto click = [&](std::string_view expected) {
        receiver.Last.clear();
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = event.button.y = 16;
        const auto handled = engine.handleEvent(event);
        CHECK(handled && *handled && receiver.Last == expected,
              "hit target and program match committed surface");
      };
      const auto initial = read();
      CHECK(initial.size() == 64 * 64 * 4 && initial[0] > initial[1],
            "baseline actually contains a red surface");
      const auto saved = engine.writeScenario();
      auto candidate = document.Surfaces;
      candidate[0].Document = "<div data-action=\"newAction()\">Omega Ω</div>";
      candidate[0].Style = "div {width:64px;height:64px;background:blue;}";
      auto invalid = candidate;
      invalid[0].Document.assign(2 * 1024 * 1024, 'x');
      CHECK(!engine.setSurfaces(invalid), "oversized markup rejected");
      CHECK(engine.writeScenario() == saved && read() == initial,
            "parse failure preserves declaration and pixels");
      click("old");
      rejectSubmit = true;
      CHECK(!engine.setSurfaces(candidate), "GPU failure rejects surface replacement");
      CHECK(rejected == 1 && !rejectSubmit, "GPU failure was reached");
      CHECK(engine.writeScenario() == saved && read() == initial,
            "GPU failure preserves declaration and pixels");
      click("old");
      CHECK(engine.setSurfaces(candidate).has_value(), "replacement retry succeeds");
      CHECK(read() != initial && engine.writeScenario() != saved,
            "successful replacement publishes different pixels and declaration");
      click("newAction");
      CHECK(engine.declare(document).has_value(),
            "redeclaration restores original surface despite render reuse");
      CHECK(read() == initial, "render declaration cache tracks setSurfaces changes");
      click("old");
      auto scrollScene = document;
      scrollScene.WheelStepPx = 16;
      scrollScene.Surfaces[0].Document = "<div id=\"scroll\"><div id=\"content\"></div></div>";
      scrollScene.Surfaces[0].Style = "#scroll {width:64px;height:64px;overflow:scroll;} #content "
                                      "{width:32px;height:128px;background:red;}";
      CHECK(engine.declare(scrollScene).has_value(), "scrolling surface ready");
      const auto unscrolled = read();
      SDL_Event wheel{};
      wheel.type = SDL_EVENT_MOUSE_WHEEL;
      wheel.wheel.mouse_x = wheel.wheel.mouse_y = 16;
      wheel.wheel.y = -1;
      rejectSubmit = true;
      CHECK(!engine.handleEvent(wheel), "failed scroll composition reports an error");
      CHECK(rejected == 2 && !rejectSubmit, "scroll failure reached submission");
      CHECK(read() == unscrolled, "failed scroll retains visible layout");
      wheel.wheel.y = 1;
      auto scrolled = engine.handleEvent(wheel);
      CHECK(scrolled && !*scrolled, "failed scroll retained the top offset");
      wheel.wheel.y = -1;
      scrolled = engine.handleEvent(wheel);
      CHECK(scrolled && *scrolled, "scroll retry succeeds");
    }
  }
  SDL_Quit();
  return Report();
}

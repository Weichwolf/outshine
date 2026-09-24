#include <Outshine.h>
#include "Check.h"

#include <SDL3/SDL.h>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the capture contract");
  {
    Engine engine;
    Scenario::Document scene;
    scene.Render.Declared = true;
    scene.Render.Frame = {64, 64};
    Scenario::View view;
    view.Id = "capture";
    view.Person = "first";
    view.Placement = Scenario::CameraPlacement::Local;
    view.Sees.PositionM = {{0, 0, 2}};
    view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
    scene.Views.push_back(view);
    CHECK(engine.setRenderTarget({64, 64}) && engine.declare(scene) && engine.assemble(),
          "assembled scene is ready for capture without streamed ground");
    {
      auto capture = engine.beginCapture();
      CHECK(capture.has_value(), "capture accepts an assembled scene without ground");
      if (!capture) { return Report(); }
      CHECK(engine.renderer().render({}).has_value(), "capture permits rendering");
      CHECK(!engine.advance(), "capture refuses simulation advancement");
      CHECK(!engine.advance(0.0), "capture refuses elapsed-time simulation advancement");
      CHECK(!engine.preload(0.0), "capture refuses streaming advancement");
      CHECK(!engine.declare(scene), "capture refuses declaration replacement");
      CHECK(!engine.assemble(), "capture refuses simulation replacement");
      CHECK(!engine.setSurfaces({}), "capture refuses overlay replacement");
      CHECK(!engine.handleEvent(SDL_Event{}), "capture refuses input state changes");
      CHECK(!engine.setGeometry(Geometry{}), "capture refuses geometry replacement");
      CHECK(!engine.setRenderTarget({32, 32}), "capture refuses target replacement");
      CHECK(!engine.setRoots({}), "capture refuses root replacement");
    }
    CHECK(engine.advance().has_value(), "capture destruction resumes simulation");
    CHECK(engine.declare(scene).has_value(), "capture destruction resumes declaration replacement");
  }
  SDL_Quit();
  return Report();
}

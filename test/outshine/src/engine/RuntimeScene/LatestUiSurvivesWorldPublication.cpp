#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "FrameReadback.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr double kNearM = 0.125;
constexpr double kHitPx = 4;
constexpr double kScrollPx = 16;

struct AtlasFont final : Ui::Font {
  const std::array<uint8_t, 4> Pixels{255, 255, 255, 255};
  mutable size_t MetricsQueries = 0;

  [[nodiscard]] Ui::FontMetrics At(Ui::FontFace face) const override {
    ++MetricsQueries;
    return Ui::AhemFont{}.At(face);
  }

  [[nodiscard]] Ui::Glyph Shape(char32_t code, Ui::FontFace face) const override {
    return Ui::AhemFont{}.Shape(code, face);
  }

  [[nodiscard]] const uint8_t *Sheet() const override { return Pixels.data(); }

  [[nodiscard]] int SheetWidthPx() const override { return 1; }

  [[nodiscard]] int SheetHeightPx() const override { return 1; }

  [[nodiscard]] uint64_t Cut() const override { return 1; }
};

Ui::SurfaceDeclaration Surface(bool blue) {
  return {.Markup = "<div><p data-action='first()'></p><p data-action='second()'></p></div>",
          .Style = std::string("div { width:32px; height:16px; overflow:scroll; }"
                               "p { margin:0; width:32px; height:16px; background:") +
                   (blue ? "#0000ff" : "#ff0000") + "; }",
          .Programme = blue ? "current" : "earlier"};
}

bool Prepare(Core::WorldCandidate &candidate,
             const Core::RuntimeScene &scene,
             const AtlasFont &font) {
  const auto prepared = candidate.Prepare(scene, &font);
  CHECK(prepared, prepared ? "the replacement prepares" : prepared.error().c_str());
  return prepared.has_value();
}

std::vector<uint8_t> Read(Render::SceneRenderer &renderer) {
  const auto published = renderer.PublishedWorld();
  renderer.WaitForGpu();
  std::vector<uint8_t> pixels;
  std::string error;
  CHECK(Render::ReadFrame(renderer, pixels, error) && !pixels.empty(),
        "the actual presentation pixels remain readable");
  return pixels;
}

void CheckUi(const Core::RuntimeScene &scene, const char *programme, const char *action) {
  size_t surface = 0;
  CHECK(scene.ProgrammeOf(0) == programme &&
            scene.Under(kHitPx, kHitPx, surface).Action == action && surface == 0,
        "programme and hit testing belong to the current live UI");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    AtlasFont font;
    Render::SceneRenderer renderer;
    const auto uploads = renderer.OfflineUploads(std::chrono::seconds(5));
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Surfaces = {Surface(false)};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, &font, scene, error),
          "the original UI world opens");
    if (!scene) { return Report(); }
    Render::Viewpoint eye;
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    eye.ZNearM = kNearM;
    eye.ZFarM = 16;
    scene->Eye(eye);
    CHECK(scene->Draw(error), "the original UI draws");
    const auto original = Read(renderer);
    CheckUi(*scene, "earlier", "first()");
    Core::WorldCandidate candidate(renderer);
    if (!Prepare(candidate, *scene, font)) { return Report(); }
    CHECK(scene->Redeclare({Surface(true)}, error), "live UI updates during world preparation");
    const auto wheeled = scene->Wheeled(kHitPx, kHitPx, kScrollPx, error);
    CHECK(wheeled && *wheeled, "live scrolling publishes during world preparation");
    CHECK(scene->Draw(error), "the new UI draws while the old candidate remains private");
    const auto latest = Read(renderer);
    CHECK(latest != original, "the colour control actually changes presentation pixels");
    CheckUi(*scene, "current", "second()");
    const size_t beforeCommit = font.MetricsQueries;
    CHECK(candidate.Publish(scene), "world replacement publishes after the UI update");
    CHECK(font.MetricsQueries == beforeCommit,
          "world publication reuses the live layout without querying font metrics again");
    CheckUi(*scene, "current", "second()");
    CHECK(!scene->Scrolled().empty() && !scene->Scrolled().front().empty() &&
              scene->Scrolled().front().front().Px == kScrollPx,
          "world publication preserves the current scroll offset");
    CHECK(scene->Draw(error) && Read(renderer) == latest,
          "world publication preserves the actual current UI pixels");
    {
      Core::WorldCandidate abandoned(renderer);
      if (!Prepare(abandoned, *scene, font)) { return Report(); }
      CHECK(scene->Redeclare({Surface(false)}, error) && scene->Draw(error),
            "live UI updates while another replacement will be abandoned");
    }
    CheckUi(*scene, "earlier", "second()");
    CHECK(Read(renderer) != latest, "candidate abandonment retains the later red UI");
    Core::WorldCandidate retry(renderer);
    if (!Prepare(retry, *scene, font)) { return Report(); }
    CHECK(scene->Redeclare({Surface(true)}, error) && scene->Draw(error),
          "the latest UI is published before a rejected world commit");
    const auto beforeFailure = Read(renderer);
    auto invalid = eye;
    invalid.XMagM = 0;
    scene->Eye(invalid);
    CHECK(!retry.Publish(scene) && Read(renderer) == beforeFailure,
          "failed view binding cannot replace the current UI pixels");
    CheckUi(*scene, "current", "second()");
    scene->Eye(eye);
    CHECK(retry.Publish(scene) && scene->Draw(error) && Read(renderer) == beforeFailure,
          "retry retains the same current UI without rebuilding it");
    CHECK(Core::RuntimeScene::Open(renderer, declaration, &font, scene, error),
          "a full new declaration deliberately replaces the previous UI");
    scene->Eye(eye);
    CHECK(scene->Draw(error) && Read(renderer) == original,
          "new declaration publishes its own earlier-coloured UI");
    CheckUi(*scene, "earlier", "first()");
  }
  SDL_Quit();
  return Report();
}

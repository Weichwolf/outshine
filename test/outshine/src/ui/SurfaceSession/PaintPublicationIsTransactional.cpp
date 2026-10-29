#include "AhemFont.h"
#include "Check.h"
#include "SurfaceSession.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine::Ui;
using namespace outshine::Test;
constexpr double kSidePx = 128;
constexpr double kHitX = 48, kHitY = 32;
constexpr double kLeftFrac = 0.25, kTopFrac = 0.125, kExtentFrac = 0.5;

struct AtlasFont final : Font {
  uint64_t Revision = 1;
  const std::array<uint8_t, 4> Pixels{255, 255, 255, 255};

  [[nodiscard]] FontMetrics At(FontFace face) const override {
    return outshine::Test::AhemFont{}.At(face);
  }

  [[nodiscard]] Glyph Shape(char32_t code, FontFace face) const override {
    return outshine::Test::AhemFont{}.Shape(code, face);
  }

  [[nodiscard]] const uint8_t *Sheet() const override { return Pixels.data(); }

  [[nodiscard]] int SheetWidthPx() const override { return 1; }

  [[nodiscard]] int SheetHeightPx() const override { return 1; }

  [[nodiscard]] uint64_t Cut() const override { return Revision; }
};

SurfaceDeclaration Surface(bool blue) {
  return {.Markup = blue ? "<div data-action='new()'></div>" : "<div data-action='kept()'></div>",
          .Style = std::string("div { width:48px; height:32px; background:") +
                   (blue ? "#0000ff" : "#ff0000") + "; }",
          .Programme = blue ? "new-script" : "kept-script",
          .LeftFrac = kLeftFrac,
          .TopFrac = kTopFrac,
          .WidthFrac = kExtentFrac,
          .HeightFrac = kExtentFrac};
}

void Exercise() {
  AtlasFont font;
  SurfaceSession session;
  std::vector<PaintQuad> published;
  unsigned accepted = 0;
  unsigned atlasAttempts = 0;
  bool accept = true;
  session.Configure(
      [&](std::span<const PaintQuad> quads, const PaintAtlasView *atlas, std::string &error) {
        if (atlas != nullptr) {
          ++atlasAttempts;
          CHECK(atlas->Rgba == font.Pixels.data() && atlas->Width == 1 && atlas->Height == 1,
                "the borrowed atlas is valid during synchronous publication");
        }
        if (!accept) {
          error = "injected paint rejection";
          return false;
        }
        published.assign(quads.begin(), quads.end());
        ++accepted;
        return true;
      },
      &font,
      {Surface(false)},
      {.WidthPx = kSidePx, .HeightPx = kSidePx});
  std::string error;
  size_t surface = 0;
  CHECK(session.Compose(error) && accepted == 1 && atlasAttempts == 1,
        "a CPU-only UI session publishes paint and its initial atlas");
  CHECK(std::ranges::any_of(published,
                            [](const PaintQuad &quad) {
                              return quad.Red == 1 && quad.Green == 0 && quad.Blue == 0 &&
                                     quad.Alpha == 1 && quad.LeftPx >= kSidePx * kLeftFrac &&
                                     quad.TopPx >= kSidePx * kTopFrac;
                            }),
        "paint preserves declared colour and surface offsets");
  CHECK(session.Under(kHitX, kHitY, surface).Action == "kept()" && surface == 0,
        "hit testing uses the published surface at its offset");
  accept = false;
  ++font.Revision;
  CHECK(!session.Redeclare({Surface(true)}, error) && error == "injected paint rejection",
        "a rejected publisher aborts the candidate");
  CHECK(accepted == 1 && session.Under(kHitX, kHitY, surface).Action == "kept()" &&
            session.ProgrammeOf(surface) == "kept-script",
        "failed paint preserves the prior hit tree and programme");
  accept = true;
  CHECK(session.Redeclare({Surface(true)}, error) && accepted == 2 && atlasAttempts == 3,
        "a rejected atlas revision is retried with the successful replacement");
  CHECK(session.Under(kHitX, kHitY, surface).Action == "new()" &&
            session.ProgrammeOf(surface) == "new-script" &&
            std::ranges::any_of(published,
                                [](const PaintQuad &quad) {
                                  return quad.Red == 0 && quad.Green == 0 && quad.Blue == 1;
                                }),
        "successful publication commits matching paint, hit tree and programme");
  CHECK(session.Compose(error) && accepted == 3 && atlasAttempts == 3,
        "an acknowledged unchanged atlas is not uploaded again");
  auto invalid = Surface(false);
  invalid.Markup = "<div>broken</br>";
  CHECK(!session.Redeclare({invalid}, error) && accepted == 3 &&
            session.Under(kHitX, kHitY, surface).Action == "new()",
        "invalid markup never reaches publication or replaces hit testing");
}
}

int main() {
  Exercise();
  outshine::Ui::SurfaceSession unconfigured;
  std::string error;
  CHECK(!unconfigured.Compose(error) && error.contains("publisher"),
        "a missing publisher is an explicit configuration error");
  return outshine::Test::Report();
}

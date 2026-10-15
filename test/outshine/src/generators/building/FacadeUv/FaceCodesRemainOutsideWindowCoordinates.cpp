#include "src/generators/building/FacadeUv.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  static_assert(FaceUvX(Facade::Wall, 0) == -1.0f);
  static_assert(FaceUvX(Facade::Wall, 1) == -17.0f);
  static_assert(FaceUvX(Facade::Plinth, 0) == -9.0f);
  static_assert(FacadeUvX(FacadeStyle::Outbuilding, Fields::Back, 0, 0.0f) == 0.0f);
  CHECK(FaceUvX(Facade::Wall, 0) < 0.0f &&
            FacadeUvX(FacadeStyle::Outbuilding, Fields::Back, 0, 0.0f) >= 0.0f,
        "plain face codes and generated window coordinates occupy separate domains");
  for (int style = 0; style < kStyleCount; ++style) {
    const auto kind = static_cast<FacadeStyle>(style);
    for (int variant = 0; variant < kFacadeVariants; ++variant) {
      const float wall = FacadeUvX(kind, Fields::Back, variant, 17.0f);
      const float pane = FacadeUvX(kind, Fields::Pane, variant, 17.0f);
      CHECK(IsGlazingUv(pane),
            "near panes preserve their source profile and separate glass response");
      CHECK(IsGlazingUv(wall) == (kind == FacadeStyle::Glazing),
            "ordinary facades keep their paint independently of the profile index");
      CHECK(static_cast<int>(wall / kBayCeil) % kStyleCount == style,
            "encoded style survives field and variant selection");
    }
  }
  CHECK(!ValidFacadeStyle(static_cast<FacadeStyle>(-1)) &&
            !ValidFacadeStyle(static_cast<FacadeStyle>(kStyleCount)),
        "invalid serialized styles cannot index the shared profile table");
  return Report();
}

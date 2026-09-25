#include "src/generators/building/FacadeUv.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  static_assert(FaceUvX(Facade::Wall, 0) == -1.0f);
  static_assert(FaceUvX(Facade::Wall, 1) == -17.0f);
  static_assert(FaceUvX(Facade::Plinth, 0) == -9.0f);
  static_assert(FacadeUvX(FacadeStyle::Outbuilding, Fields::Back, 0.0f) == 0.0f);
  CHECK(FaceUvX(Facade::Wall, 0) < 0.0f &&
            FacadeUvX(FacadeStyle::Outbuilding, Fields::Back, 0.0f) >= 0.0f,
        "plain face codes and generated window coordinates occupy separate domains");
  return Report();
}

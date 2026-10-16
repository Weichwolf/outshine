#include "src/client/PlaceFrameProfile.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<double, 7> rates{0.0,
                                        -1.0,
                                        29.97,
                                        120.0,
                                        std::numeric_limits<double>::infinity(),
                                        -std::numeric_limits<double>::infinity(),
                                        std::numeric_limits<double>::quiet_NaN()};
  for (const double rate : rates) {
    const auto profile = Shots::FrameProfile::ForPlace(rate);
    CHECK(!profile.has_value() && !profile.error().empty(),
          "invalid or unsupported measurement rates fail before allocation or division");
  }
  const Shots::FrameProfile defaultProfile;
  CHECK(defaultProfile.RateHz() == 60 && defaultProfile.Frames() == 60,
        "the existing sixty-frame measurement remains the default");
  return Report();
}

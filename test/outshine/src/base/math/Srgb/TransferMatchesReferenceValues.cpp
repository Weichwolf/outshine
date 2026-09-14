#include "math/Srgb.h"
#include "Check.h"
#include <array>
#include <cmath>

int main() {
  using namespace outshine::Test;
  using namespace outshine::ColourSpace;

  struct Reference {
    float Encoded;
    double Linear;
  };

  constexpr std::array references{Reference{0, 0},
                                  Reference{1, 1},
                                  Reference{0.5f, 0.21404114048223255},
                                  Reference{0.25f, 0.05087608817155679},
                                  Reference{0.75f, 0.5225215539683921}};
  // Float operations versus double tabulations: a few float ulps over normalized RGB.
  constexpr double tolerance = 3e-7;
  for (const auto &reference : references) {
    CHECK(std::abs(LinearFromSrgb(reference.Encoded) - reference.Linear) <= tolerance,
          "decode agrees with fixed standard transfer values");
    CHECK(std::abs(SrgbFromLinear(static_cast<float>(reference.Linear)) - reference.Encoded) <=
              tolerance,
          "encode agrees with fixed standard transfer values");
  }
  CHECK(std::abs(LinearFromSrgb(0.04045f) - 0.0031308049535603715) < 1e-9,
        "encoded knee belongs to the linear branch");
  CHECK(std::abs(SrgbFromLinear(0.0031308f) - 0.040449936) < 1e-8,
        "linear knee uses the standard linear segment");
  float previous = -1;
  for (int byte = 0; byte < 256; ++byte) {
    const double encoded = byte / 255.0;
    const double reference =
        encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
    const float linear = LinearFromSrgb(static_cast<float>(encoded));
    CHECK(std::abs(linear - reference) <= tolerance, "all byte codes match double reference");
    CHECK(linear > previous, "decoding preserves strict byte ordering");
    CHECK(std::lround(SrgbFromLinear(linear) * 255) == byte,
          "every byte survives decode and encode without changing its code");
    previous = linear;
  }
  return Report();
}

#ifndef OUTSHINE_TEST_RENDER_AIM_H
#define OUTSHINE_TEST_RENDER_AIM_H

#include <cstdint>

#include <Scenario.h>

namespace outshine::Test {

// WHERE A POINT LANDS, WITH THE DOOR'S OWN MATRIX. `Camera::clipMatrix` composes the view and the
// projection, so the two things left here are a multiply and a viewport map -- arithmetic with no
// policy in it, which is why this runner may own them. Everything that DECIDES anything about the
// camera stays behind the door, and both formulas below are the engine's to the last term.
struct Clip {
  double M[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  [[nodiscard]] bool Stands(const Camera &of, double aspect) { return of.clipMatrix(aspect, M); }

  void Point(const double point[3], double out[3]) const {
    const double w = M[3] * point[0] + M[7] * point[1] + M[11] * point[2] + M[15];
    const double scale = (w != 0.0) ? 1.0 / w : 1.0;
    for (int row = 0; row < 3; ++row) {
      out[row] =
          (M[row] * point[0] + M[4 + row] * point[1] + M[8 + row] * point[2] + M[12 + row]) * scale;
    }
  }
};

struct Frame {
  double WidthPx = 0;
  double HeightPx = 0;

  [[nodiscard]] double Aspect() const { return (HeightPx > 0) ? WidthPx / HeightPx : 0.0; }
  void Raster(const double ndc[3], double outPx[2]) const {
    outPx[0] = (ndc[0] * 0.5 + 0.5) * WidthPx - 0.5;
    outPx[1] = (0.5 - ndc[1] * 0.5) * HeightPx - 0.5;
  }
};

}

#endif

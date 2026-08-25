#ifndef OUTSHINE_CONTENT_SHADE_EXPOSUREPARAMS_H
#define OUTSHINE_CONTENT_SHADE_EXPOSUREPARAMS_H

namespace outshine {

enum class ExposureMode { Auto, Manual };

struct ExposureParams {
  ExposureMode Mode = ExposureMode::Auto;

  float KeyEv = 0.0f;
  float CompEv = 0.0f;
};

}
#endif

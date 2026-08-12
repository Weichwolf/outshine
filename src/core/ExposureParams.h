#ifndef EXPOSUREPARAMS_H
#define EXPOSUREPARAMS_H

namespace outshine {

enum class ExposureMode { Auto, Manual };

/* Stops, never lux: the HDR scene target is scene-referred in top-of-atmosphere-solar units and has
 * no photometric calibration anywhere, so an EV100 field here would be a number with nothing under
 * it. Both modes end in a SLIDE of the same curve, so a scene can move the exposure and cannot bend
 * it. Declaring nothing is the normal case and is why mods/demo/scene.json has no exposure block.
 *
 * There are no adaptation time constants and there will not be: the anchor is a function of the
 * illumination alone, and the viewer in front of the screen brings his own adaptation state. */
struct ExposureParams {
  ExposureMode Mode = ExposureMode::Auto;
  /* Manual: the scene radiance, log2, that shall sit at the adaptation luminance. */
  float KeyEv = 0.0f;
  float CompEv = 0.0f;   /* Auto: stops on top of the irradiance-derived placement */
};

} // namespace outshine
#endif

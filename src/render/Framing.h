#ifndef OUTSHINE_RENDER_FRAMING_H
#define OUTSHINE_RENDER_FRAMING_H

// HOW A CAMERA IS PLACED TO FRAME A THING, and it is a CAMERA question rather than a file-format
// one. These constants sat in the glTF importer's namespace because the function that uses them did
// -- `FramingFor` fits a viewpoint to a bounding box, which is geometry and arithmetic and knows
// nothing about a file. The render tier owns cameras, so it owns these.
namespace outshine::Render {

constexpr double kFramingAzimuthDeg = 35.0;
constexpr double kFramingElevationDeg = 20.0;

constexpr double kFramingSensorHalfHeightMm = 12.0;
constexpr double kFramingFocalLengthMm = 50.0;

constexpr double kFramingFill = 0.6;

constexpr double kFramingNearFloorFraction = 0.001;

} // namespace outshine::Render
#endif

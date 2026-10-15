#version 450
#extension GL_GOOGLE_include_directive : require
#include "mediumViewAir.glsl"
#include "mediumOpaque.glsl"
#include "cloud_air_prefix.glsl"

#ifndef CLOUD_VIEW_DROP_ORTHO_OFFSET
#define CLOUD_VIEW_DROP_ORTHO_OFFSET 0
#endif
#ifndef CLOUD_VIEW_CLIP_PARTITION
#define CLOUD_VIEW_CLIP_PARTITION 0
#endif
#ifndef CLOUD_VIEW_DOUBLE_FRONT
#define CLOUD_VIEW_DOUBLE_FRONT 0
#endif
#ifndef CLOUD_VIEW_DROP_CAMERA_ANCHOR
#define CLOUD_VIEW_DROP_CAMERA_ANCHOR 0
#endif
#ifndef CLOUD_VIEW_CLEAR_OPAQUE_CONTROL
#define CLOUD_VIEW_CLEAR_OPAQUE_CONTROL 0
#endif
#ifndef CLOUD_VIEW_MOMENT_REPLAY
#define CLOUD_VIEW_MOMENT_REPLAY 0
#endif

layout(location = 0) out vec4 colour;
layout(location = 1) out vec4 radianceDepth;
layout(location = 2) out vec4 transmittanceDistance;
layout(location = 3) out vec4 work;
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D depth;
layout(set = 2, binding = 2) uniform sampler2D multiScatter;
layout(set = 2, binding = 3) uniform sampler2D veil;

layout(std140, set = 3, binding = 2) uniform MediumFrame {
  MediumView View;
}

frame;

layout(std140, set = 3, binding = 3) uniform WeatherFrame {
  CloudLayer Layer;
  vec4 DensityCameraOriginM;
  vec4 SunIrradiance;
  vec4 SkyRadiance;
}

weather;

void main() {
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  vec4 lit = texelFetch(scene, pixel, 0);
  float written = texelFetch(depth, pixel, 0).r;
  radianceDepth = vec4(0.0, 0.0, 0.0, written);
  transmittanceDistance = vec4(1.0, 1.0, 1.0, 0.0);
  work = vec4(0.0);
  if (written <= 0.0) {
    colour = lit;
    return;
  }
  MediumViewRay view = mediumViewRay(frame.View, vec3(gl_FragCoord.xy, written));
  AirTransfer result;
  if (weather.Layer.field.x <= 0.0) {
    result = mediumViewAir(frame.View, view, veil, multiScatter);
    work.w = -1.0;
  } else {
    AirPathSpan span = airPhysicalSpan(frame.View.Air, view.Air);
    if (span.EndKm <= span.BeginKm || view.Air.DistanceKm <= span.BeginKm) {
      result = AirTransfer(vec3(0.0), vec3(1.0));
    } else {
      AirRay air = airPathRay(frame.View.Air, view.Air, span);
      AirPrefix clear = airPrefixBegin(frame.View.Air, air, veil, multiScatter);
      vec3 originM = weather.DensityCameraOriginM.xyz;
#if CLOUD_VIEW_DROP_CAMERA_ANCHOR
      originM.xz = vec2(0.0);
#endif
#if !CLOUD_VIEW_DROP_ORTHO_OFFSET
      originM += view.OriginOffsetM;
#endif
      float physicalExtentM = max(0.0, span.EndKm * 1000.0);
#if CLOUD_VIEW_CLIP_PARTITION
      physicalExtentM = min(physicalExtentM, view.Air.DistanceKm * 1000.0);
#endif
      CloudLight light = CloudLight(
          normalize(frame.View.SunDir.xyz), weather.SunIrradiance.rgb, weather.SkyRadiance.rgb);
      CloudAirPrefix prefix = cloudAirPrefixBegin(weather.Layer,
                                                  originM,
                                                  view.Direction,
                                                  physicalExtentM,
                                                  light,
                                                  clear,
                                                  span.BeginKm * 1000.0);
#if CLOUD_AIR_MOMENTS && CLOUD_VIEW_MOMENT_REPLAY
      cloudAirPrefixAdvance(prefix, view.Air.DistanceKm * 500.0, frame.View.Air, veil, multiScatter);
      cloudAirPrefixAdvance(prefix, view.Air.DistanceKm * 500.0, frame.View.Air, veil, multiScatter);
#endif
      result = cloudAirPrefixAdvance(
          prefix, view.Air.DistanceKm * 1000.0, frame.View.Air, veil, multiScatter);
#if CLOUD_AIR_MOMENTS && CLOUD_VIEW_MOMENT_REPLAY
      result = cloudAirPrefixAdvance(
          prefix, view.Air.DistanceKm * 1000.0, frame.View.Air, veil, multiScatter);
#endif
      work = vec4(prefix.Cloud.DensityEvaluations,
                  prefix.Cloud.SourceEvaluations,
                  prefix.CellsVisited,
                  prefix.Clear.SourceEvaluations);
#if CLOUD_AIR_MOMENTS
      work = vec4(prefix.QueryMoments, prefix.Cloud.DensityEvaluations, prefix.CellsVisited);
#endif
    }
  }
  vec3 combined = lit.rgb * result.Transmittance + frame.View.Illuminance * result.RadiancePerLux;
#if CLOUD_VIEW_DOUBLE_FRONT
  combined = combined * result.Transmittance + frame.View.Illuminance * result.RadiancePerLux;
#endif
  colour = vec4(combined, lit.a);
#if CLOUD_VIEW_CLEAR_OPAQUE_CONTROL
  colour = mediumClearOpaque(frame.View, gl_FragCoord.xy, scene, depth, multiScatter, veil);
#endif
  radianceDepth.rgb = result.RadiancePerLux;
  transmittanceDistance = vec4(result.Transmittance, view.Air.DistanceKm);
}

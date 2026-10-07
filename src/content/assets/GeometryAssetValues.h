#ifndef OUTSHINE_CONTENT_ASSETS_GEOMETRYASSETVALUES_H
#define OUTSHINE_CONTENT_ASSETS_GEOMETRYASSETVALUES_H

#include "scene/Material.h"
#include "scene/PunctualLight.h"

namespace outshine::Content {
template <class Archive> bool GeometryAssetMap(Archive &archive, SurfaceMap &map) {
  return archive(map.Image,
                 map.Set,
                 map.Sampler.Magnify,
                 map.Sampler.Minify,
                 map.Sampler.Mip,
                 map.Sampler.WrapU,
                 map.Sampler.WrapV,
                 map.Uv.OffsetUv.Axis,
                 map.Uv.ScaleUv.Axis,
                 map.Uv.RotationRad);
}

template <class Archive> bool GeometryAssetMaterial(Archive &archive, Material &surface) {
  if (!archive(surface.Pattern,
               surface.BaseColour.Axis,
               surface.Metalness,
               surface.Roughness,
               surface.Transmission,
               surface.Ior,
               surface.Emission.Axis,
               surface.Alpha,
               surface.DoubleSided,
               surface.CoverageCut,
               surface.Unlit,
               surface.NeedsTangents,
               surface.NormalScale,
               surface.OcclusionStrength,
               surface.SpecularFactor,
               surface.SpecularColour.Axis,
               surface.SheenColour.Axis,
               surface.SheenRoughness,
               surface.Clearcoat,
               surface.ClearcoatRoughness,
               surface.Anisotropy,
               surface.AnisotropyRotationRad,
               surface.Iridescence,
               surface.IridescenceIor,
               surface.IridescenceThicknessMinNm,
               surface.IridescenceThicknessMaxNm,
               surface.Thickness,
               surface.AttenuationDistance,
               surface.AttenuationColour.Axis)) {
    return false;
  }
  for (auto *map : {&surface.BaseColourMap,
                    &surface.NormalMap,
                    &surface.MetalRoughMap,
                    &surface.EmissiveMap,
                    &surface.OcclusionMap,
                    &surface.SpecularStrengthMap,
                    &surface.SpecularTintMap}) {
    if (!GeometryAssetMap(archive, *map)) { return false; }
  }
  return true;
}

template <class Archive> bool GeometryAssetLight(Archive &archive, PunctualLight &light) {
  return archive(light.Kind,
                 light.Colour.Axis,
                 light.Intensity,
                 light.Position.Axis,
                 light.Direction.Axis,
                 light.InnerConeRad,
                 light.OuterConeRad,
                 light.RangeM);
}
}
#endif

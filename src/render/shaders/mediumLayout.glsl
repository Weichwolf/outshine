struct MediumUv { float U; float V; };
struct MediumLutSize { float WidthPx; float HeightPx; };
struct MediumLook { float RadiusKm; float CosZenith; };
struct SkyViewLook { float CosView; float LightViewCos; };

struct Medium {
  float BottomRadiusKm; float TopRadiusKm; float RayleighScaleHeightKm; float MieScaleHeightKm;
  vec3 RayleighScatteringPerKm; float MieScatteringPerKm;
  vec3 OzoneAbsorptionPerKm; float MieExtinctionPerKm;
  float OzoneCentreKm; float OzoneHalfWidthKm; float MiePhaseG; float pad;
  vec3 GroundAlbedo; float pad2;
};

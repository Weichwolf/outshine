#version 410 core
#include "medium.glsl"
#include "skyViewLookup.glsl"
#include "display.glsl"
in vec2 ndc;
out vec4 colour;
uniform int stage;
uniform vec2 extent;
uniform float eyeKm;
uniform vec3 sun;
uniform vec3 rightRay;
uniform vec3 upRay;
uniform vec3 forward;
uniform sampler2D transmittance;
uniform sampler2D multiScatter;
uniform sampler2D skyView;
void main() {
  Medium air = Medium(6360.,6460.,8.,1.2,vec3(.005802,.013558,.033100),0.,
      vec3(.000650,.001881,.000085),0.,25.,15.,.8,0.,vec3(.10,.13,.07),0.);
  vec2 uv=gl_FragCoord.xy/extent;
  MediumUv coord=MediumUv(uv.x,uv.y);
  MediumLutSize size=MediumLutSize(extent.x,extent.y);
  if(stage==0){
    MediumLook ray=mediumTransmittanceTexel(air,coord,size);
    colour=vec4(mediumTransmittanceToTop(air,ray,kTransmittanceSteps),1.);
  } else if(stage==1){
    vec3 lum,transfer;
    mediumMultiScatterTexel(air,multiScatterParams(air,coord,size,kMediumGroundLiftKm),transmittance,
      kMultiScatterSteps,kMultiScatterGrid,kMediumLuminanceSegment,kMediumGroundLiftKm,lum,transfer);
    colour=vec4(lum/(1.-transfer),1.);
  } else if(stage==2){
    SkyViewLook ray=skyViewParams(air,eyeKm,coord,size);
    colour=vec4(mediumSkyRay(air,eyeKm,ray,sun.z,transmittance,multiScatter,kSkyViewSteps,
      kMediumLuminanceSegment,kMediumGroundLiftKm),1.);
  } else {
    vec3 dir=normalize(forward+ndc.x*rightRay+ndc.y*upRay);
    ivec2 res=textureSize(skyView,0);
    SkyViewSample sampleAt=skyViewSample(air.BottomRadiusKm,eyeKm,
      SkyViewLook(dir.z,skyLightViewCos(dir,vec3(0.,0.,1.),sun)), MediumLutSize(float(res.x),float(res.y)));
    vec3 rgb=texture(skyView,vec2(sampleAt.Uv.U,sampleAt.Uv.V)).rgb;
    if(!sampleAt.HitsGround && dot(dir,sun)>cos(kSunHalfAngleRad)){
      rgb+=mediumTransmittanceToTop(air,MediumLook(eyeKm,dir.z),kTransmittanceSteps)/
        (OUTSHINE_PI*kSunHalfAngleRad*kSunHalfAngleRad);
    }
    colour=vec4(pow(filmic(rgb*(2.5/1.2)),vec3(1./2.2)),1.);
  }
}

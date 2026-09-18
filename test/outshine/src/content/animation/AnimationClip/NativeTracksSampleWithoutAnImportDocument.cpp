#include "AnimationClip.h"
#include "Check.h"

#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  std::vector<std::unique_ptr<AnimationTrack>> tracks;
  auto translation = std::make_unique<AnimationTrack>();
  translation->Target = 0;
  translation->Property = AnimationTarget::Translation;
  translation->Times = {0.0, 2.0};
  translation->Values = {0.0, 0.0, 0.0, 4.0, 6.0, 8.0};
  CHECK(AnimationCurve::Build(Keyframes::Interpolation::Linear,
                              translation->Times,
                              translation->Values,
                              3,
                              AnimationCurve::Values::Linear,
                              translation->Curve),
        "native translation curve built");
  tracks.push_back(std::move(translation));

  auto colour = std::make_unique<AnimationTrack>();
  colour->Target = 3;
  colour->Property = AnimationTarget::BaseColour;
  colour->Times = {0.0, 2.0};
  colour->Values = {0.0, 0.2, 0.4, 1.0, 1.0, 0.8, 0.6, 1.0};
  CHECK(AnimationCurve::Build(Keyframes::Interpolation::Linear,
                              colour->Times,
                              colour->Values,
                              4,
                              AnimationCurve::Values::Linear,
                              colour->Curve),
        "native material curve built");
  tracks.push_back(std::move(colour));

  auto emission = std::make_unique<AnimationTrack>();
  emission->Target = 4;
  emission->Property = AnimationTarget::Emission;
  emission->Times = {0.0, 2.0};
  emission->Values = {0.0, 0.0, 0.0, 1.0, 0.5, 0.25};
  emission->EmissionScale = 8.0;
  CHECK(AnimationCurve::Build(Keyframes::Interpolation::Linear,
                              emission->Times,
                              emission->Values,
                              3,
                              AnimationCurve::Values::Linear,
                              emission->Curve),
        "native emission curve built");
  tracks.push_back(std::move(emission));

  AnimationClip clip;
  clip.Adopt(
      std::vector<AnimationRestPose>(1), {}, std::move(tracks), {.StartS = 0.0, .EndS = 2.0});
  std::vector<AffineTransform> pose;
  std::vector<double> weights;
  clip.SamplePose(1.0, pose, weights);
  CHECK((pose.size() == 1 && pose[0].M.Translation() == Vec3{{2.0, 3.0, 4.0}}),
        "native pose samples independently of an import document");

  std::vector<AnimatedMaterialSample> materials;
  clip.SampleMaterials(1.0, materials);
  CHECK((materials.size() == 2 && materials[0].Material == 3 &&
         materials[0].Property == AnimationTarget::BaseColour &&
         materials[0].Values == Vec4{{0.5, 0.5, 0.5, 1.0}}),
        "native material targets retain their engine meaning");
  CHECK((materials[1].Material == 4 && materials[1].Property == AnimationTarget::Emission &&
         materials[1].Values == Vec4{{4.0, 2.0, 1.0, 0.0}}),
        "native emission carries its radiance scale without an import document");
  return Report();
}

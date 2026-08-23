#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "Check.h"

#include "DrawList.h"
#include "RenderPlan.h"
#include "Renderer.h"

namespace {

// a unit quad facing the camera, no tangent run anywhere -- the layout the defect hid on
constexpr float kVerts[12] = {-1.f, -1.f, -3.f, 1.f, -1.f, -3.f, 1.f, 1.f, -3.f, -1.f, 1.f, -3.f};
constexpr float kNormals[12] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f};
constexpr float kUv[8] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
constexpr uint32_t kIndices[6] = {0, 1, 2, 0, 2, 3};
constexpr float kEmitted[12] = {};

// glTF's metal-rough packing: roughness in G, metalness in B -- this texel says
// "rough 0.5, dielectric" against factors that say "rough 1, metal"
constexpr uint8_t kOrmTexels[16] = {255, 128, 0, 255, 255, 128, 0, 255,
                                    255, 128, 0, 255, 255, 128, 0, 255};

[[nodiscard]] bool RadianceSum(outshine::Render::Renderer &renderer, bool withImage,
                               double &sum, std::string &error, bool glass = false) {
  outshine::Render::SubjectMaterial surface;
  surface.Row.BaseColour[0] = surface.Row.BaseColour[1] = surface.Row.BaseColour[2] = 0.8f;
  surface.Row.BaseColour[3] = 1.0f;
  surface.Row.Metalness = 1.0f;
  surface.Row.Roughness = 1.0f;
  if (glass) {
    surface.Row.Metalness = 0.0f;
    surface.Row.Transmission = 0.9f;
  }
  if (withImage) {
    surface.MetalRough.Rgba = kOrmTexels;
    surface.MetalRough.Width = 2;
    surface.MetalRough.Height = 2;
  }
  if (!renderer.SetSubjectMaterials({&surface, 1}, error)) { return false; }

  outshine::Render::SubjectLight light;
  light.Light.Kind = outshine::LightKind::Directional;
  light.Light.Intensity = 3.14159265f;
  light.Light.Colour[0] = light.Light.Colour[1] = light.Light.Colour[2] = 1.0f;
  light.Light.Direction[0] = 0.0f;
  light.Light.Direction[1] = 0.0f;
  light.Light.Direction[2] = -1.0f;
  if (!renderer.SetSubjectLights({&light, 1}, error)) { return false; }
  outshine::Render::SubjectEnvironment sky;
  sky.RadianceLinear[0] = sky.RadianceLinear[1] = sky.RadianceLinear[2] = 0.2;
  renderer.SetSubjectEnvironment(sky);

  outshine::Render::DrawList draws;
  outshine::Render::DrawItem item;
  item.Order.Viewport = 0;
  item.Order.Layer = outshine::Render::ViewLayer::World;
  item.Order.Surface = surface.State();
  item.Order.DepthFraction = 0.5;
  item.Order.MaterialSlot = 0;
  item.ModelSlot = 0;
  item.SourceFirstIndex = 0;
  item.IndexCount = 6;
  item.Layout = outshine::Render::VertexLayout::PositionNormalUv;
  if (!draws.Add(item, error)) { return false; }
  draws.Compile();

  outshine::Render::SubjectMesh mesh;
  mesh.Verts = kVerts;
  mesh.Normals = kNormals;
  mesh.Uv = kUv;
  mesh.Emitted = kEmitted;
  mesh.VertexCount = 4;
  mesh.Indices = kIndices;
  mesh.IndexCount = 6;
  mesh.Draws = &draws;
  if (!renderer.SetSubjectMesh(mesh, error)) { return false; }
  const double identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  if (!renderer.SetSubjectPlacements(identity, 1, error)) { return false; }

  const double eye[3] = {0, 0, 0}, fwd[3] = {0, 0, -1}, right[3] = {1, 0, 0}, up[3] = {0, 1, 0};
  renderer.SetFovDeg(45.0f);
  renderer.SetNearM(0.05f);
  renderer.SetCameraBasis(eye, fwd, right, up);

  renderer.BeginTemporalRun();
  renderer.RenderFrame();
  std::vector<float> linear;
  if (renderer.ReadSceneLinear(linear) != outshine::Render::ReadState::Ready) {
    error = "the scene did not read back";
    return false;
  }
  sum = 0.0;
  for (size_t at = 0; at + 3 < linear.size(); at += 4) {
    const double r = linear[at], g = linear[at + 1], b = linear[at + 2];
    if (std::isfinite(r) && std::isfinite(g) && std::isfinite(b)) { sum += r + g + b; }
  }
  return true;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Render::PlanSpec declaration;
  declaration.Outputs = {outshine::Render::Resource::SceneDepth,
                         outshine::Render::Resource::FrameTex};
  declaration.Content = {outshine::Render::Stage::Subjects,
                         outshine::Render::Stage::SubjectsTransmissive,
                         outshine::Render::Stage::CompositeTransmission};
  declaration.Display =
      outshine::Render::Declared<outshine::Render::Transfer>(outshine::Render::Transfer::Linear);
  declaration.Exposure = outshine::Render::Declared<float>(1.0f);
  declaration.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
      outshine::Render::ScenePrecision::Float);
  std::shared_ptr<const outshine::Render::RenderPlan> plan;
  std::string error;
  CHECK([&] { auto made = outshine::Render::RenderPlan::Compile(declaration); if (made) { plan = *std::move(made); return true; } error = std::move(made).error(); return false; }(),
        "the instrument's render declaration compiles");
  if (!plan) { return Report(); }

  outshine::Render::Renderer renderer;
  renderer.Init(64, 64, plan);
  if (!renderer.DeviceUsable()) {
    Unprepared("no usable device -- this proof needs the GPU");
    return Report();
  }

  double withImage = 0.0, factorsOnly = 0.0;
  const bool a = RadianceSum(renderer, true, withImage, error);
  if (!a) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(a, "the quad with a metal-rough image and NO tangent frame renders");
  const bool b = RadianceSum(renderer, false, factorsOnly, error);
  if (!b) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(b, "and the same quad renders again on its factors alone");

  Note("radiance with the image", withImage, "summed linear");
  Note("radiance on factors alone", factorsOnly, "summed linear");
  CHECK(withImage > 0.0 && factorsOnly > 0.0, "both frames drew something");
  const double apart = std::fabs(withImage - factorsOnly) /
                       (factorsOnly > 0.0 ? factorsOnly : 1.0);
  Note("how far the two frames are apart", apart, "relative");
  CHECK(apart > 0.10,
        "**A LIT TEXTURED PART READS THE METAL-ROUGH IMAGE ITS MATERIAL DECLARES, WHATEVER "
        "THE TANGENT FRAME SAYS**: the image says dielectric rough 0.5 against factors "
        "saying metal rough 1, and the frames differ -- the arm that ignored the sampler "
        "rendered these identically (board:1145)");

  {
    // the same class one arm over: a TRANSMISSIVE lit textured part without a tangent
    // frame reads its metal-rough image too (board:1725)
    double glassWith = 0.0, glassWithout = 0.0;
    const bool c = RadianceSum(renderer, true, glassWith, error, true);
    if (!c) { std::printf("REFUSED %s\n", error.c_str()); }
    const bool d = RadianceSum(renderer, false, glassWithout, error, true);
    CHECK(c && d && glassWith > 0.0 && glassWithout > 0.0, "both glass frames drew");
    const double glassApart = std::fabs(glassWith - glassWithout) /
                              (glassWithout > 0.0 ? glassWithout : 1.0);
    Note("how far the two glass frames are apart", glassApart, "relative");
    CHECK(glassApart > 0.05,
          "**THE LIT TRANSMISSIVE ARM READS THE IMAGES ITS MATERIAL DECLARES**: the "
          "roughness image moves the glass's shading -- the untextured arm rendered these "
          "identically (board:1725)");
  }

  Covers("V.6 the metal-rough tap belongs to textured-and-lit, not to has-a-tangent: glTF "
         "ties the two textures together nowhere, and a material with a metal-rough image "
         "and no normal map shades from the image on the tangentless layout (board:1145)");
  return Report();
}

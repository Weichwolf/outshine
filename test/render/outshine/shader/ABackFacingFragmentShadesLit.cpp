#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "DrawList.h"
#include "RenderPlan.h"
#include "Renderer.h"

namespace {

// the same tangentless quad the metal-rough proof stands, DOUBLE-SIDED, seen from behind:
// the rasteriser hands front_facing = false, facing() must flip the normal toward the
// camera, and the light stands on the CAMERA'S side so the flipped normal is LIT -- a
// back-lit case would come back black and prove only that the branch was entered
constexpr float kVerts[12] = {-1.f, -1.f, -3.f, 1.f, -1.f, -3.f, 1.f, 1.f, -3.f, -1.f, 1.f, -3.f};
constexpr float kNormals[12] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f};
constexpr float kUv[8] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
constexpr uint32_t kIndices[6] = {0, 1, 2, 0, 2, 3};
constexpr float kEmitted[12] = {};

struct Shaded {
  size_t Front = 0;
  size_t Back = 0;
  double Radiance = 0.0;
};

[[nodiscard]] bool Render(outshine::Render::Renderer &renderer, bool fromBehind, Shaded &out,
                          std::string &error) {
  outshine::Render::SubjectMaterial surface;
  surface.Row.BaseColour[0] = surface.Row.BaseColour[1] = surface.Row.BaseColour[2] = 0.8f;
  surface.Row.BaseColour[3] = 1.0f;
  surface.Row.Metalness = 0.0f;
  surface.Row.Roughness = 0.6f;
  surface.Row.DoubleSided = true;
  if (!renderer.SetSubjectMaterials({&surface, 1}, error)) { return false; }

  outshine::Render::SubjectLight light;
  light.Light.Kind = outshine::LightKind::Directional;
  light.Light.Intensity = 3.14159265f;
  light.Light.Colour[0] = light.Light.Colour[1] = light.Light.Colour[2] = 1.0f;
  light.Light.Direction[0] = 0.0f;
  light.Light.Direction[1] = 0.0f;
  // the light TRAVELS with the camera's view: from behind, it travels +Z onto the far
  // face the flipped normal presents; from the front, -Z onto the near face
  light.Light.Direction[2] = fromBehind ? 1.0f : -1.0f;
  if (!renderer.SetSubjectLights({&light, 1}, error)) { return false; }
  outshine::Render::SubjectEnvironment sky;
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

  const double sign = fromBehind ? -1.0 : 1.0;
  const double eye[3] = {0, 0, fromBehind ? -6.0 : 0.0};
  const double fwd[3] = {0, 0, -sign};
  const double right[3] = {sign, 0, 0};
  const double up[3] = {0, 1, 0};
  renderer.SetFovDeg(45.0f);
  renderer.SetNearM(0.05f);
  renderer.SetCameraBasis(eye, fwd, right, up);

  renderer.BeginTemporalRun();
  renderer.RenderFrame();
  std::vector<float> normal;
  if (renderer.ReadShadingNormal(normal) != outshine::Render::ReadState::Ready) {
    error = "the shading-normal channel did not read back";
    return false;
  }
  std::vector<float> linear;
  if (renderer.ReadSceneLinear(linear) != outshine::Render::ReadState::Ready) {
    error = "the scene did not read back";
    return false;
  }
  out = Shaded{};
  for (size_t at = 0; at + 3 < normal.size(); at += 4) {
    const double length = std::sqrt((double)normal[at] * normal[at] +
                                    (double)normal[at + 1] * normal[at + 1] +
                                    (double)normal[at + 2] * normal[at + 2]);
    if (!(length > 0.0)) { continue; }
    if (normal[at + 3] < 0.0f) {
      ++out.Back;
    } else {
      ++out.Front;
    }
    if (at < linear.size()) {
      out.Radiance += (double)linear[at] + linear[at + 1] + linear[at + 2];
    }
  }
  return true;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Render::PlanSpec declaration;
  declaration.Outputs = {outshine::Render::Resource::SceneDepth,
                         outshine::Render::Resource::FrameTex,
                         outshine::Render::Resource::SceneShadingNormal};
  declaration.Content = {outshine::Render::Stage::Subjects};
  declaration.Display =
      outshine::Render::Declared<outshine::Render::Transfer>(outshine::Render::Transfer::Linear);
  declaration.Exposure = outshine::Render::Declared<float>(1.0f);
  declaration.Precision = outshine::Render::Declared<outshine::Render::ScenePrecision>(
      outshine::Render::ScenePrecision::Float);
  std::shared_ptr<const outshine::Render::RenderPlan> plan;
  std::string error;
  CHECK(outshine::Render::RenderPlan::Compile(declaration, &plan, error),
        "the instrument's render declaration compiles");
  if (!plan) { return Report(); }

  outshine::Render::Renderer renderer;
  renderer.Init(64, 64, plan);
  if (!renderer.DeviceUsable()) {
    Unprepared("no usable device -- this proof needs the GPU");
    return Report();
  }

  Shaded front, back;
  const bool a = Render(renderer, false, front, error);
  if (!a) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(a, "the quad renders from the front");
  const bool b = Render(renderer, true, back, error);
  if (!b) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(b, "and from behind");

  Note("front view: front-facing shaded", (double)front.Front, "px");
  Note("front view: back-facing shaded", (double)front.Back, "px");
  Note("behind view: back-facing shaded", (double)back.Back, "px");
  Note("behind view: front-facing shaded", (double)back.Front, "px");
  Note("behind view: radiance", back.Radiance, "summed linear");

  CHECK(front.Front > 0 && front.Back == 0,
        "from the front every shaded fragment is front-facing -- the baseline");
  CHECK(back.Back > 0 && back.Front == 0,
        "**A BACK-FACING FRAGMENT SHADES**: seen from behind, the whole shaded population "
        "is back-facing -- the rasteriser's bool reaches the branch that flips, on the arm "
        "that draws (board:1147)");
  CHECK(back.Radiance > 0.1 * front.Radiance,
        "**AND IT IS LIT, NOT MERELY PRESENT**: the light stands on the camera's side, the "
        "flipped normal faces it, and the picture carries radiance -- a black population "
        "would prove only that the branch was entered");

  Covers("V.7 the back branch of the lit facing wire is proven end to end in a picture: a "
         "double-sided quad seen from behind shades its whole population back-facing and "
         "LIT, both counts published side by side (board:1147)");
  return Report();
}

/* THE SETUP API FOR A STUDIO WHOSE SUBJECT IS A glTF DOCUMENT: no world, no place, no tile stream,
 * no wire. It is the one place that decides where a declaration's frame sits inside the engine's,
 * and it is a CONSUMER-FACING call rather than something `src/render/` does for itself -- the
 * renderer draws what it is handed and knows nothing about what the model is of.
 *
 * TWO FRONT ENDS, ONE CALL. A C++ consumer calls `Show` directly; a scenario loader that declares a
 * glTF subject calls the same `Show` with what it read. Whatever a declaration can express, code can
 * express, because there is only one thing to express it to.
 *
 * IT CONFIGURES AND RETURNS, AND IT STORES NOTHING (`F.20`, `R.3`). Anything that has to keep acting
 * over time -- a clock, a keyframed camera -- is engine state and belongs in the engine; a setup
 * object holding a reference to the thing it configured would imply it stays alive to do something
 * later, and it does not. It refuses by naming what it could not satisfy, in this tree's style: no
 * throw, a bool and a sentence (`E.27` where exceptions are not the house error channel here). */
#ifndef GLTFSTUDIO_H
#define GLTFSTUDIO_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "DrawList.h"
#include "Subject.h"
#include "SubjectDraw.h"

namespace outshine::Render {
class Renderer;
}

namespace outshine::Clients {

/* WHERE A STUDIO SITS ON THE GLOBE, and the answer is that the question is not the declaration's.
 * The renderer's frame is ECEF, so a camera basis has to be built at SOME geodetic point; the null
 * island at zero height is the one choice that is a datum rather than a preference, and it is the
 * same one `src/scenario/Studio.h` makes for the subject bench.
 *
 * At lat 0, lon 0 the local basis is exactly east = +Y, north = +Z, up = +X, so the map from glTF's
 * frame is a permutation with no trigonometry in it: +X to east, +Y to up, and glTF's forward -Z to
 * north. Its determinant is +1 -- a mirror here would flip every silhouette's handedness and would be
 * invisible in a coverage number over a symmetric subject. */
constexpr double kStudioAnchorEcefM[3] = {6378137.0, 0.0, 0.0}; /* WGS84 semi-major axis */

void EcefFromGltf(const double gltf[3], double out[3]);

/* WHAT A STUDIO IS, as one parameter object rather than five arguments (`I.23`): the geometry, where
 * it is seen from, what each of its parts emits and which surface each part wears. The appearance is
 * the DECLARATION's -- a Lambertian facet of linear albedo rho under a uniform environment of
 * radiance L emits rho*L, an emissive surface emits its own colour -- and nothing in `src/render/`
 * invents either number.
 *
 * ONE RADIANCE AND ONE SURFACE SLOT PER PART, AND `Show` REFUSES ANY OTHER COUNT. A part is one
 * drawn primitive, so a declaration that gave one colour to three touching bodies, or one material
 * to two primitives that name different ones, cannot be written: it is the wrong length and it is
 * named as such. */
struct Studio {
  const Gltf::Subject *Geometry = nullptr;
  Gltf::Placement Eye;
  /* Scene-referred linear radiance, one RGB triple per `Geometry->Parts()`, in that order. */
  std::vector<std::array<float, 3>> EmittedRadiance;
  /* Which surface slot each part draws with, one per `Geometry->Parts()`. It is the draw key's own
   * material field, so the consumer decides how many distinct surfaces there are -- two parts of one
   * material share a slot and are then batchable. */
  std::vector<uint32_t> PartSurface;
  /* The surfaces themselves, indexed by slot. A surface whose colour image has no texels declares
   * no texture, which is a different pipeline and not a white stand-in. */
  std::vector<Render::SubjectMaterial> Surfaces;
  /* THE SAME SUBJECT AT THE PREVIOUS FRAME, or null where there was no previous one (board:1169).
   * It is what makes `SceneVelocity` a motion: the pose is baked into a subject's world positions,
   * so the previous pose cannot be recovered from a transform and has to arrive as a second
   * subject. It must carry the same vertices in the same order -- the same document flattened at
   * another time -- and `Show` refuses any other count.
   *
   * IT IS DECLARED EXACTLY WHERE THE PLAN ATTACHES A VELOCITY TARGET and refused where it does not:
   * a previous pose nobody reads is a run uploaded per frame for nothing, and an attachment with no
   * previous pose is a target something asked for and nothing wrote. */
  const Gltf::Subject *Previous = nullptr;
  /* WHAT LIGHTS THE SUBJECT, IN glTF's OWN FRAME AND UNITS -- right-handed, +Y up, metres, with a
   * directional light's intensity in lux and a point or spot light's in candela. Empty is the
   * ordinary state and it is not "unlit by default": it says the declaration lights nothing, so
   * every part draws the radiance the declaration gave it and no cosine is evaluated anywhere. */
  std::vector<outshine::PunctualLight> Lights;
  /* THE ENVIRONMENT THIS SUBJECT GATHERS: a constant radiance from every direction, scene-referred,
   * and ZERO unless the consumer declares one (board:1206). It sits beside the light list because it
   * is a light -- the one whose solid angle is the whole sphere -- and a consumer that states an
   * environment and a consumer that states none write the same call. */
  Render::SubjectEnvironment Environment;
};

/* THE CALLER'S WORKSPACE, so a loop over many cases reuses one set of buffers -- the hot-loop
 * exception `F.20` states for itself. Nothing in it survives the call as meaning; it is capacity. */
struct StudioScratch {
  std::vector<float> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<Render::SubjectLight> Lights;
  Render::DrawList Draws;
};

/* MOVES THE EYE AND NOTHING ELSE, which is what a camera path over a standing subject is. It is
 * separate from `Show` because the two answer different questions on different clocks: the geometry
 * is set up once and the eye moves every frame, and a caller that had to restate the whole studio
 * to turn the camera would rebuild the mesh sixty times a second.
 *
 * `Show` CALLS THIS RATHER THAN RESTATING IT, so the projection, the near-plane refusal and the
 * basis mapping have one spelling. Refuses a placement declaring no lens, an orthographic one whose
 * two magnifications disagree with the frame's aspect, and an eye with the subject inside the
 * engine's near plane -- naming the numbers. */
[[nodiscard]] bool Aim(Render::Renderer &renderer, const Gltf::Subject &subject,
                       const Gltf::Placement &eye, std::string &error);

/* Places the subject and the eye, compiles its draw list and hands the renderer the mesh. Refuses a
 * subject with no triangle, a declaration whose per-part arrays are the wrong length, a surface slot
 * no table entry answers, and a placement whose eye is inside the engine's near plane -- naming the
 * numbers in every case. */
[[nodiscard]] bool Show(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
                        std::string &error);

/* **WHAT THE SUBJECT IS MADE OF, SET ONCE** (board:1463): its surfaces, its lights and the environment
 * it sits in. `SetSubjectMaterials` retires the whole table and re-uploads every image, so a consumer
 * that restated this per frame paid the subject's entire texture set on every advance -- and nothing
 * about a material changes when a body moves. `Show` is this followed by `Pose`, which is what a runner
 * drawing one frame per subject wants; a scenario calls the two apart. */
[[nodiscard]] bool Surface(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
                           std::string &error);

/* **THE BODY AT ONE POSE**, and the verb is the compositor's own from `CLAUDE.md` -- place, cull,
 * quantise, draw: the draw list, the index run, the packed vertices and the mesh, plus the
 * camera through `Aim`. It refuses a subject with no triangle, per-part arrays of the wrong length, a
 * previous pose of a different vertex count and an eye inside the near plane -- naming the numbers. */
[[nodiscard]] bool Place(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
                        std::string &error);

} // namespace outshine::Clients
#endif

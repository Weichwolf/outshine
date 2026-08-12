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
#include <string>
#include <vector>

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
 * it is seen from, and what each of its parts emits. The appearance is the DECLARATION's -- a
 * Lambertian facet of linear albedo rho under a uniform environment of radiance L emits rho*L, an
 * emissive surface emits its own colour -- and nothing in `src/render/` invents either number.
 *
 * ONE RADIANCE PER PART, AND `Show` REFUSES ANY OTHER COUNT. The subject's parts are its
 * mesh-bearing nodes, so a declaration that gave one colour to three touching bodies cannot be
 * written: it is the wrong length and it is named as such. */
struct Studio {
  const Gltf::Subject *Geometry = nullptr;
  Gltf::Placement Eye;
  /* Scene-referred linear radiance, one RGB triple per `Geometry->Parts()`, in that order. */
  std::vector<std::array<float, 3>> EmittedRadiance;
  /* The decoded base colour, when the declaration names one. A texture with no texels is a subject
   * that declares none, which is a different pipeline and not a white stand-in. */
  Render::SubjectTexture BaseColour;
};

/* Places the subject and the eye, and hands the renderer the mesh. `scratch` is the caller's so a
 * loop over many cases reuses one buffer -- the hot-loop exception `F.20` states for itself.
 * Refuses a subject with no triangle and a placement whose eye is inside the engine's near plane,
 * naming both numbers. */
[[nodiscard]] bool Show(Render::Renderer &renderer, const Studio &studio,
                        std::vector<float> &scratch, std::string &error);

} // namespace outshine::Clients
#endif

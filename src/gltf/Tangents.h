/* THE PER-VERTEX TANGENT BASIS A FILE DID NOT SUPPLY, in the one algorithm the format names:
 * "When tangents are not specified, client implementations SHOULD calculate tangents using default
 * MikkTSpace algorithms with the specified vertex positions, normals, and texture coordinates
 * associated with the normal texture" (Specification.adoc:1426).
 *
 * IT IS MikkTSpace AND NOT A PER-TRIANGLE FIT, and the difference is the whole reason the format
 * names an algorithm rather than a formula. A per-triangle basis averaged per vertex depends on the
 * order the triangles were visited and on how a tool split its uv islands, so two conforming readers
 * of one file disagree along every seam. Mikkelsen's construction removes both: vertices are welded
 * by their whole attribute tuple, the triangles around a welded vertex are grouped by connectivity
 * AND by the sign of their uv parametrisation, and each group's basis is the angle-weighted mean of
 * its members'. The bakers -- Blender, xNormal, Substance -- run this same construction, so a mesh
 * whose normal map was baked against it comes back the way it was baked.
 *
 * THE SECOND TEXTURE COORDINATE IS FLIPPED ON THE WAY IN, and that is the one convention decision
 * here. glTF's v runs DOWN the image while the OpenGL normal-map convention the format mandates has
 * the green channel pointing UP, so the basis MikkTSpace is defined against is the one over `(u,
 * -v)`. MEASURED against `NormalTangentMirrorTest`, whose `TANGENT` Blender's own exporter wrote:
 * without the flip every one of its 2770 tangents comes back with the opposite `w`, and the tangent
 * direction itself is unchanged -- because flipping v negates both the parametric derivative and the
 * orientation sign, and the two cancel in the direction and do not in the handedness.
 *
 * A BASIS IS PRODUCED PER TRIANGLE CORNER AND NOT PER VERTEX, because that is what the algorithm
 * yields: two corners that reference one vertex may sit in different groups and then hold different
 * bases. Folding them back into an indexed run is the caller's, and the only correct fold is to
 * split the vertex -- averaging there would put back exactly the smoothing decision this algorithm
 * exists to take away. */
#ifndef GLTF_TANGENTS_H
#define GLTF_TANGENTS_H

#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Gltf {

/* THE MESH ONE BASIS IS GENERATED OVER: parallel vertex runs and one triangle run, as one parameter
 * object rather than six arguments (`I.23`). The runs are the caller's and are not owned here
 * (`R.3`). `Uv` is glTF's own, v down; the flip is this unit's and happens once, inside. */
struct TangentSubject {
  const double *PositionsM = nullptr; /* 3 per vertex */
  const double *Normals = nullptr;    /* 3 per vertex, unit */
  const double *Uv = nullptr;         /* 2 per vertex, glTF's own v-down convention */
  size_t VertexCount = 0;
  const uint32_t *Indices = nullptr; /* 3 per triangle, into the runs above */
  size_t IndexCount = 0;
};

/* One basis per triangle CORNER -- 4 doubles, a unit tangent and glTF's handedness `w`, so that
 * `bitangent = cross(normal, tangent.xyz) * w` (Specification.adoc:1420). `out` is resized to
 * `4 * IndexCount`. Refuses a subject whose runs do not agree with each other, naming both counts:
 * a basis generated over the wrong normal is a picture nobody can attribute. */
[[nodiscard]] bool GenerateTangents(const TangentSubject &subject, std::vector<double> &out,
                                    std::string &error);

} // namespace outshine::Gltf
#endif

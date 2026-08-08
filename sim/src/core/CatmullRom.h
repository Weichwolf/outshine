#ifndef CATMULLROM_H
#define CATMULLROM_H

#include <cstddef>

namespace outshine {

/* TANGENTS FROM BARE VALUES — the operation the keyframe evaluator deliberately does NOT contain
 * (F.2). Two of the three consumers have no tangents of their own: a path from a search is a
 * polyline, and an OSM way is a polyline, and both kink at every point. Only a scene channel brings
 * glTF tangents with it.
 *
 * Both entry points write into a CALLER-PROVIDED buffer and allocate nothing: the heaviest consumer
 * is the bake, over hundreds of thousands of points. */

/* Knots under the centripetal (alpha = 0.5) / chordal (1.0) / uniform (0.0) parameterisation:
 * t0 = 0, t[i+1] = t[i] + |p[i+1] - p[i]|^alpha. `knotsOut` holds `count` numbers.
 *
 * CENTRIPETAL IS THE ONE TO USE FOR POSITIONS, and the reason is measured elsewhere in this tree:
 * world/ClassField.cpp's CurveRing() smooths OSM ways with alpha = 0.5 because the uniform form
 * loops and overshoots wherever the points are unevenly spaced. A SCALAR channel over a strictly
 * increasing frame axis cannot loop, so a scene channel passes its frames straight in instead. */
void CurveKnots(const double *points, size_t count, size_t components, double alpha,
                double *knotsOut);

/* Catmull-Rom tangents as glTF triples — in-tangent, value, out-tangent per keyframe, per
 * component, which is exactly the layout Keyframes::Interpolation::CubicSpline reads. `triplesOut`
 * holds count * 3 * components numbers. The ends get a one-sided difference, which is what keeps
 * the first and last segment from being flat. */
void CatmullRomTangents(const double *knots, size_t count, const double *values, size_t components,
                        double *triplesOut);

} // namespace outshine
#endif

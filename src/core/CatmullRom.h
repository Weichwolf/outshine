#ifndef CATMULLROM_H
#define CATMULLROM_H

#include <cstddef>

namespace outshine {

void CurveKnots(const double *points, size_t count, size_t components, double alpha,
                double *knotsOut);

void CatmullRomTangents(const double *knots, size_t count, const double *values, size_t components,
                        double *triplesOut);

}
#endif

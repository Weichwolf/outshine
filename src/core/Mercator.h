/* WHERE THE MAP STOPS, stated once for the whole tree. atan(sinh(pi)) in degrees is the WMTS / OGC
 * simple-tile-scheme bound: beyond it there is no tile at any zoom and no caller may invent one.
 *
 * It is here rather than beside the tile lattice because a DECLARATION has to refuse a latitude the
 * scheme cannot carry before anything streams, and the declaration layer may not name the world. */
#ifndef MERCATOR_H
#define MERCATOR_H

namespace outshine {

constexpr double kMercatorLatMaxDeg = 85.05112877980659;

} // namespace outshine
#endif

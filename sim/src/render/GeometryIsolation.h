/* FB_GEOM=1: judge GEOMETRY without judging light. Owner, 2026-08-06: "da Nanite Geometrie ist wuerde
 * ich es ohne Postprocessing und Schatten bauen." One flag rather than three, because a measurement
 * that forgot one of them is worse than no measurement.
 *
 * It disarms RECEIVERS and freezes numbers; it removes no pass and no draw, so the per-frame
 * Begin*Pass count is identical with and without. */
#ifndef GEOMETRYISOLATION_H
#define GEOMETRYISOLATION_H

#include <cstdlib>

namespace outshine::Render {

[[nodiscard]] inline bool GeometryIsolation(void) {
  static const int on = []() { const char *e = getenv("FB_GEOM"); return e && atoi(e) != 0 ? 1 : 0; }();
  return on != 0;
}

} // namespace outshine::Render
#endif

#include "Wayfinding.h"

namespace outshine::World {

Route Plan(const Waypoint &from, const Waypoint &to) {
  Route out;
  out.Error = "no graph is built over the streamed ways yet, so no route between " +
              std::to_string(from.LatDeg) + " " + std::to_string(from.LonDeg) + " and " +
              std::to_string(to.LatDeg) + " " + std::to_string(to.LonDeg) +
              " can be planned -- StreetField holds ways per tile and nothing joins them at their "
              "shared nodes, nothing carries a link length, and nothing searches. board:1503";
  return out;
}

} // namespace outshine::World

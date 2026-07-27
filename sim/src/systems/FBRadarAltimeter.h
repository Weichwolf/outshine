/* FlightBox — FBRadarAltimeter: AGL aus dem bereits aufgeloesten elev/ground-Paar der App, und der
 * Referenzfall des Busses fuer "Invalid". doc/flightbox/systems.md, Abschnitt 5. */
#ifndef FBRADARALTIMETER_H
#define FBRADARALTIMETER_H

#include "FBState.h"

namespace FlightBox {

class FBRadarAltimeter {
public:
  virtual ~FBRadarAltimeter() = default;

  void SetPowered(bool on) { Powered_ = on; }
  bool Powered() const { return Powered_; }

  /* elevAslM/groundAslM: Flugzeug- und DEM-Boden-ASL in Metern, dasselbe Paar wie fuer SetAgl. */
  virtual void Run(FBState &state, float elevAslM, float groundAslM);

private:
  bool Powered_ = true;
};

} // namespace FlightBox
#endif

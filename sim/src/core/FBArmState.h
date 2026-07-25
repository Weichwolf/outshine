/* FlightBox — FBArmState: master-arm status the HUD's ARM/SIM line reads. Lives in core/ (like
 * FBMode/FBMasterMode) because FBState carries it and multiple layers (a module's weapon-system
 * override, the HUD) need the same enum without core/ reaching into systems/modules. */
#ifndef FBARMSTATE_H
#define FBARMSTATE_H

namespace FlightBox {

enum class FBArmState { Sim, Arm };

} // namespace FlightBox
#endif

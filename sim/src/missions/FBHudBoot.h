/* THE ONE DOOR from a mod's `.fbh` into systems/FBHudDeck — the file half of a declared HUD, kept
 * out of systems/ for the same reason FBCatalogueBoot is kept out of core/: the layers below the
 * clients are I/O-free. The manifest names the file, this reads it, and the engine names no title. */
#ifndef FBHUDBOOT_H
#define FBHUDBOOT_H

#include <string>

#include "FBHudDecl.h"

namespace FlightBox::Missions {

bool FBLoadHud(const std::string &path, Systems::FBHudDeck &out, std::string *err);

} // namespace FlightBox::Missions
#endif

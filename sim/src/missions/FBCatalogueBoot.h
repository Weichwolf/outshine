/* THE ONE DOOR from a mod's catalogue manifest into core/FBAircraftCatalogue — the file half of
 * doc/mods.md §3, kept out of core/ because core/ is I/O-free. Same rule as every other root: the
 * manifest names the path (FBMod::Catalogue), this reads it, and the engine names no airframe.
 *
 * A row's numbers are its own; what an omitted field means is core/FBAircraft.h's declared default, so
 * a manifest states departures rather than repeating the schema. Anything the schema does not know is
 * an ERROR with its line number and never a silently dropped line — a typo that costs a row would
 * surface much later as a mission that cannot spawn. */
#ifndef FBCATALOGUEBOOT_H
#define FBCATALOGUEBOOT_H

#include <string>
#include "FBAircraftCatalogue.h"

namespace FlightBox::Missions {

bool FBLoadAircraftCatalogue(const std::string &path, FBAircraftCatalogue &out, std::string *err);

} // namespace FlightBox::Missions
#endif

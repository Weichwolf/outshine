/* ...and what a declaration MAY name: core's value types, and its own directory. */
#include "Mod.h"
#include "Standpoint.h"
int main() {
  const outshine::Scenario::Mod mod;
  return outshine::Scenario::Standpoint::At(0.0, 0.0) && mod.Ids().empty() ? 0 : 1;
}

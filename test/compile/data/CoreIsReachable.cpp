/* ...and what a provider MAY name: core's value types, and its own directory. */
#include "Sha256.h"
#include "SourceSet.h"
#include "TerrariumDem.h"
int main() {
  const outshine::Data::TerrariumDem dem;
  return dem.Declaration().MaxZoom > 0 && !outshine::Sha256Hex(std::string("x")).empty() ? 0 : 1;
}

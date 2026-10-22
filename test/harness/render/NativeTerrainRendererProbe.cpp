#include "NativeTerrainRendererProbe.h"

int main(int argc, char **argv) {
  return outshine::Test::GroundProbe::RunTerrainRendererProbe(argc > 1 ? argv[1] : "");
}

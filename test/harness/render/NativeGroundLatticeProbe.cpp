#include "NativeGroundLatticeProbe.h"

int main(int argc, char **argv) {
  return outshine::Test::GroundProbe::RunGroundLatticeProbe(argc > 1 ? argv[1] : "");
}

#include "BuiltinGenerators.h"
#include "Check.h"
#include "StructureMesher.h"
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::shared_ptr<const StructureMesher> mesher;
  {
    const Generators::BuiltinGenerators generators;
    mesher = generators.BuildingMesher();
    CHECK(mesher && mesher == generators.BuildingMesher(),
          "the catalogue provides a stable shared mesher instead of recreating its recipes");
  }
  CHECK(mesher && mesher->HasSurfaceProjection() && mesher->Scratch(),
        "the native building mesher and its scratch factory remain valid after catalogue teardown");
  return Report();
}

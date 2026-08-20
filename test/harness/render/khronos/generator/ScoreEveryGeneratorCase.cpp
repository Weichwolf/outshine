/* THE HARNESS FOR the Khronos glTF-Asset-Generator's animation groups, fetched at the pin every
 * manifest cites (board:1458).
 *
 * IT IS A FOLDER'S OWN RUNNER AND NOT A FLAG ON A SHARED ONE (board:1196): the corpus, its cases and
 * the binary that scores them sit together, so a suite can be run, moved or retired as one thing. The
 * measurement itself is shared -- a case is decided the same way whoever authored the asset. */
#include "RenderCase.h"

int main(int argc, char **argv) { return ScoreRenderCase(argc, argv); }

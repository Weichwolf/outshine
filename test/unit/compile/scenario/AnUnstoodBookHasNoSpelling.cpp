#include "Views.h"
#include "Tables.h"
#include "Triggers.h"
// REFUSED: calling a private constructor
// the three books hold ONE invariant each -- the active view exists, a keyed row is unique,
// a door names a declared event -- and a default-constructed book holds none of them while
// answering as if it did. The factory is the only spelling, so an unstood book cannot reach
// an accessor at all: the type system refuses it where a bool verdict only warned.
int main(void) {
  outshine::ViewBook unstood;
  outshine::TableBook alsoUnstood;
  outshine::TriggerField stillUnstood;
  return 0;
}

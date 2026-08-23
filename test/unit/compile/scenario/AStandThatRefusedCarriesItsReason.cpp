#include "Views.h"
// REFUSED: nodiscard
// a stand-up that refused hands back its reason, and dropping that value on the floor is
// the defect the bool verdict allowed -- the warning set makes it an error.
int main(void) {
  outshine::View one;
  one.Id = "chase";
  one.Follows = "car";
  one.Person = "third";
  one.TimeScale = 1.0;
  const outshine::View declared[1] = {one};
  outshine::ViewBook::Stand(declared, "chase");
  return 0;
}

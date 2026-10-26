#include "Script.h"
#include <cmath>

namespace outshine::Script {

bool Value::Truth() const {
  switch (What) {
    case Kind::Nothing:
    case Kind::Null: return false;
    case Kind::Number:
    case Kind::Boolean: return Number != 0.0 && !std::isnan(Number);
    case Kind::Text: return !Text.empty();
    case Kind::Ref: return true;
  }
  return false;
}

}

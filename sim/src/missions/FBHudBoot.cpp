#include "FBHudBoot.h"

#include <fstream>
#include <sstream>

namespace FlightBox::Missions {

bool FBLoadHud(const std::string &path, Systems::FBHudDeck &out, std::string *err) {
  std::ifstream in(path);
  if (!in) {
    if (err) *err = "cannot open " + path;
    return false;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  std::string why;
  if (!Systems::FBParseHud(buf.str(), out, &why)) {
    if (err) *err = path + ": " + why;
    return false;
  }
  return true;
}

} // namespace FlightBox::Missions

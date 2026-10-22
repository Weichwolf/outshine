#include "TransformMatrix.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <array>

int main(int argc, const char *const *argv) {
  if (argc != 3) { return 2; }
  std::ifstream input(argv[1]);
  auto frame = nlohmann::json::parse(input, nullptr, false);
  if (frame.is_discarded()) { return 2; }
  outshine::Mat4 clip;
  clip.Column = frame["camera_clip"].get<std::array<double, 16>>();
  outshine::Mat4 inverse;
  if (!outshine::InverseMatrix(clip, inverse)) { return 2; }
  frame["inverse_camera_clip"] = inverse.Column;
  std::ofstream output(argv[2]);
  output << frame.dump(2) << '\n';
  return output ? 0 : 2;
}

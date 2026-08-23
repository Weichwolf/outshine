#include "ShaderFile.h"

#include <cstdio>

namespace outshine::Render {

bool LoadShaderText(std::string_view treePath, std::string &into, std::string &error) {
  const std::string path(treePath);
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    error = "the shader source " + path +
            " is not readable from here -- the engine reads its shaders from the tree, so the "
            "process must start at the repository root";
    return false;
  }
  into.clear();
  char block[1 << 14];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  if (into.empty()) {
    error = "the shader source " + path + " is empty, and an empty kernel is a picture refusal";
    return false;
  }
  return true;
}

}

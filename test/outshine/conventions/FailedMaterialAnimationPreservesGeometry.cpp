#include <import/GltfImporter.h>
#include "Check.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto pattern =
      (std::filesystem::temp_directory_path() / "outshine-material-failure-XXXXXX").string();
  if (mkdtemp(pattern.data()) == nullptr) {
    Unprepared("fixture directory unavailable");
    return Report();
  }

  struct Cleanup {
    std::filesystem::path Path;

    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(Path, ignored);
    }
  } cleanup{pattern};

  const auto &root = cleanup.Path;
  const std::array<float, 27> data{0, 0, 0, 1, 0, 0,    0, 1, 0, 0, 1,  1, 0, 0,
                                   1, 0, 0, 1, 1, 0.5f, 2, 0, 0, 2, 10, 0, 2};
  {
    std::ofstream file(root / "data.bin", std::ios::binary);
    file.write(reinterpret_cast<const char *>(data.data()), sizeof(data));
    CHECK(file.good(), "fixture bytes written");
  }
  {
    std::ofstream file(root / "scene.gltf");
    file << R"({
  "asset": {
    "version": "2.0"
  },
  "extensionsUsed": [
    "KHR_animation_pointer"
  ],
  "buffers": [
    {
      "uri": "data.bin",
      "byteLength": 108
    }
  ],
  "bufferViews": [
    {
      "buffer": 0,
      "byteLength": 36
    },
    {
      "buffer": 0,
      "byteOffset": 36,
      "byteLength": 8
    },
    {
      "buffer": 0,
      "byteOffset": 44,
      "byteLength": 32
    },
    {
      "buffer": 0,
      "byteOffset": 76,
      "byteLength": 8
    },
    {
      "buffer": 0,
      "byteOffset": 84,
      "byteLength": 24
    },
    {
      "buffer": 0,
      "byteOffset": 80,
      "byteLength": 4
    }
  ],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "min": [
        0,
        0,
        0
      ],
      "max": [
        1,
        1,
        0
      ]
    },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 2,
      "type": "SCALAR",
      "min": [
        0
      ],
      "max": [
        1
      ]
    },
    {
      "bufferView": 2,
      "componentType": 5126,
      "count": 2,
      "type": "VEC4"
    },
    {
      "bufferView": 3,
      "componentType": 5126,
      "count": 2,
      "type": "SCALAR"
    },
    {
      "bufferView": 4,
      "componentType": 5126,
      "count": 2,
      "type": "VEC3"
    },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 1,
      "type": "SCALAR",
      "min": [
        0
      ],
      "max": [
        0
      ]
    },
    {
      "bufferView": 5,
      "componentType": 5126,
      "count": 1,
      "type": "SCALAR"
    }
  ],
  "materials": [
    {},
    {}
  ],
  "meshes": [
    {
      "primitives": [
        {
          "attributes": {
            "POSITION": 0
          },
          "material": 0
        }
      ]
    }
  ],
  "nodes": [
    {
      "mesh": 0
    },
    {
      "camera": 0
    }
  ],
  "scenes": [
    {
      "nodes": [
        0,
        1
      ]
    }
  ],
  "scene": 0,
  "animations": [
    {
      "samplers": [
        {
          "input": 1,
          "output": 2
        },
        {
          "input": 1,
          "output": 3
        },
        {
          "input": 1,
          "output": 4
        }
      ],
      "channels": [
        {
          "sampler": 0,
          "target": {
            "path": "pointer",
            "extensions": {
              "KHR_animation_pointer": {
                "pointer": "/materials/0/pbrMetallicRoughness/baseColorFactor"
              }
            }
          }
        },
        {
          "sampler": 1,
          "target": {
            "path": "pointer",
            "extensions": {
              "KHR_animation_pointer": {
                "pointer": "/materials/1/pbrMetallicRoughness/roughnessFactor"
              }
            }
          }
        },
        {
          "sampler": 2,
          "target": {
            "node": 1,
            "path": "translation"
          }
        }
      ]
    },
    {
      "samplers": [
        {
          "input": 5,
          "output": 6
        }
      ],
      "channels": [
        {
          "sampler": 0,
          "target": {
            "path": "pointer",
            "extensions": {
              "KHR_animation_pointer": {
                "pointer": "/materials/1/pbrMetallicRoughness/roughnessFactor"
              }
            }
          }
        }
      ]
    }
  ],
  "cameras": [
    {
      "type": "perspective",
      "perspective": {
        "yfov": 1,
        "znear": 0.1
      }
    }
  ]
})";
    CHECK(file.good(), "fixture declaration written");
  }
  GltfImporter asset;
  const auto loaded = asset.load((root / "scene.gltf").string());
  CHECK(loaded.has_value(), "fixture loads at valid initial time");
  if (!loaded) { return Report(); }
  const std::array clips{0};
  CHECK(asset.selectAnimations(clips).has_value(), "valid initial sample selected");
  const Material first = asset.geometry().surfaceAt(MaterialInstance(0));
  const Material second = asset.geometry().surfaceAt(MaterialInstance(1));
  const float *positions = asset.geometry().positionsOf(0).data();
  CHECK(first.BaseColour[0] == 1 && second.Roughness == 0.5f,
        "initial sample matches independent values");
  Camera before;
  CHECK(asset.camera(0, before) && before.PositionM[0] == 0, "initial camera matches fixture");
  CHECK(!asset.sampleAnimation(1), "later invalid factor rejects sample");
  CHECK(asset.geometry().surfaceAt(MaterialInstance(0)) == first &&
            asset.geometry().surfaceAt(MaterialInstance(1)) == second,
        "failed later factor preserves both previous materials");
  CHECK(asset.geometry().positionsOf(0).data() == positions,
        "failed sample retains borrowed geometry storage");
  Camera after;
  CHECK(asset.camera(0, after) && after.PositionM == before.PositionM,
        "failed sample preserves indexed camera pose");
  CHECK(asset.camera().PositionM == before.PositionM, "default camera agrees with indexed camera");
  const std::array invalidClip{1};
  CHECK(!asset.selectAnimations(invalidClip), "clip with invalid initial material is rejected");
  CHECK(asset.durationS() == 1, "failed clip selection retains previous clip duration");
  CHECK(asset.sampleAnimation(0.25).has_value(),
        "previous clip remains usable after failed selection");
  CHECK(asset.camera(0, after) && after.PositionM[0] == 2.5,
        "successful retry publishes matching camera pose");
  return Report();
}

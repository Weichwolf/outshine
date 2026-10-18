"""Private two-gather alternative for the original five-point height stencil."""

from native_tree_detail_world import replace_once


GATHER = """vec3 gatheredGroundHeight(sampler2DArray pages, uint page, ivec2 node) {
  const uint side = uint(kGroundSide + 2);
  const uint perLayer = kGroundPageColumns * kGroundPageColumns;
  const uvec2 offset = uvec2(page % kGroundPageColumns, (page % perLayer) / kGroundPageColumns) * side;
  const ivec2 at = ivec2(offset) + clamp(node, ivec2(0), ivec2(kGroundSide - 1)) + 1;
  const float scale = 1.0 / float(side * kGroundPageColumns);
  const float layer = float(page / perLayer);
  const vec4 plus = textureGather(pages, vec3(vec2(at + 1) * scale, layer), 0);
  const vec4 minus = textureGather(pages, vec3(vec2(at) * scale, layer), 0);
  return vec3(plus.w, plus.z - minus.x, minus.z - plus.x);
}

"""


def lattice_source(source):
    source = replace_once(source, "float groundNodeAt(", GATHER + "float groundNodeAt(")
    source = replace_once(source,
                          "  const float h = groundNodeAt(pages, page, i, j);",
                          "  const vec3 gathered = gatheredGroundHeight(pages, page, ivec2(i, j));\n"
                          "  const float h = gathered.x;")
    source = replace_once(source,
                          "(groundNodeAt(pages, page, i + 1, j) - groundNodeAt(pages, page, i - 1, j))",
                          "gathered.y")
    return replace_once(source,
                        "(groundNodeAt(pages, page, i, j - 1) - groundNodeAt(pages, page, i, j + 1))",
                        "gathered.z")


def normal_source(source):
    source = replace_once(source, "float fineGroundHeight(", GATHER + "float fineGroundHeight(")
    return replace_once(source,
                        "  const float de = fineGroundHeight(page, node + ivec2(1, 0)) - fineGroundHeight(page, node - ivec2(1, 0));\n"
                        "  const float dn = fineGroundHeight(page, node - ivec2(0, 1)) - fineGroundHeight(page, node + ivec2(0, 1));",
                        "  const vec3 gathered = gatheredGroundHeight(heightPages, page, node);\n"
                        "  const float de = gathered.y;\n"
                        "  const float dn = gathered.z;")

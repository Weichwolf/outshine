"""Private shared-cell gather alternative for fine terrain normals and shadows."""

from native_tree_detail_world import replace_once

SAMPLES = """struct FineGroundSamples {
  vec4 heights;
  vec4 east;
  vec4 north;
};

FineGroundSamples gatherFineGroundSamples() {
  if ((groundSampleCode & kGroundLodMask) == 0u) {
    return FineGroundSamples(vec4(0.0), vec4(0.0), vec4(0.0));
  }
  const uint code = groundSampleCode & ~(kGroundUniformBit | kGroundLodMask);
  const uint tile = (code >> kGroundPatchBits) * 7u;
  const uint page = uint(groundTiles[tile + 6u].x);
  const vec2 at = clamp(groundSampleUV * float(kGroundSide - 1), vec2(0.0), vec2(float(kGroundSide - 1)));
  const ivec2 cell = min(ivec2(floor(at)), ivec2(kGroundSide - 2));
  const uint side = uint(kGroundSide + 2);
  const uint perLayer = kGroundPageColumns * kGroundPageColumns;
  const uvec2 offset = uvec2(page % kGroundPageColumns, (page % perLayer) / kGroundPageColumns) * side;
  const ivec2 origin = ivec2(offset) + cell + 1;
  const float scale = 1.0 / float(side * kGroundPageColumns);
  const float layer = float(page / perLayer);
  const vec4 nw = textureGather(heightPages, vec3(vec2(origin) * scale, layer), 0);
  const vec4 ne = textureGather(heightPages, vec3(vec2(origin + ivec2(2, 0)) * scale, layer), 0);
  const vec4 sw = textureGather(heightPages, vec3(vec2(origin + ivec2(0, 2)) * scale, layer), 0);
  const vec4 se = textureGather(heightPages, vec3(vec2(origin + 2) * scale, layer), 0);
  return FineGroundSamples(
      vec4(nw.y, ne.x, se.w, sw.z),
      vec4(ne.x - nw.x, ne.y - nw.y, se.z - sw.z, se.w - sw.w),
      vec4(nw.z - sw.z, ne.w - se.w, ne.x - se.x, nw.y - sw.y));
}

vec3 fineGroundSampleNormal(float east, float north, vec2 stepM, mat3 basis) {
  return normalize(basis * normalize(vec3(-vec2(east, north) / (2.0 * stepM), 1.0)));
}

"""


def sources(shader_root):
    normal = (shader_root / "groundFineNormal.glsl").read_text()
    overload = normal[normal.index("vec3 groundFineNormal()") :]
    overload = overload.replace(
        "groundFineNormal()", "groundFineNormal(FineGroundSamples samples)", 1
    )
    for node, component in (
        ("cell", "x"),
        ("cell + 1", "z"),
        ("cell + ivec2(1, 0)", "y"),
        ("cell + ivec2(0, 1)", "w"),
    ):
        overload = replace_once(
            overload,
            f"fineGroundNodeNormal(page, {node}, stepM, basis)",
            f"fineGroundSampleNormal(samples.east.{component}, samples.north.{component}, stepM, basis)",
        )
    normal = replace_once(
        normal, "vec3 fineGroundNodeNormal(", SAMPLES + "vec3 fineGroundNodeNormal("
    )
    normal += "\n" + overload
    shadow = (shader_root / "groundFineShadow.glsl").read_text()
    shadow = replace_once(
        shadow, "fineGroundPoint(uint page,", "fineGroundPoint(float height,"
    )
    shadow = replace_once(shadow, "fineGroundHeight(page, node)", "height")
    shadow = replace_once(
        shadow, "fineGroundShadow()", "fineGroundShadow(vec4 heights)"
    )
    for node, component in (
        ("cell", "x"),
        ("cell + 1", "z"),
        ("cell + ivec2(1, 0)", "y"),
        ("cell + ivec2(0, 1)", "w"),
    ):
        shadow = replace_once(
            shadow,
            f"fineGroundPoint(page, {node},",
            f"fineGroundPoint(heights.{component}, {node},",
        )
    fragment = (shader_root / "groundLit.glsl").read_text()
    fragment = replace_once(
        fragment,
        "  vec3 surfaceNormal = groundFineNormal();\n#if SUBJECT_BASE_LOCATION == 0\n"
        "  vec4 shadowLightSpace = fineGroundShadow();\n#endif",
        "#if SUBJECT_BASE_LOCATION == 0\n"
        "  const FineGroundSamples fineSamples = gatherFineGroundSamples();\n"
        "  vec3 surfaceNormal = groundFineNormal(fineSamples);\n"
        "  vec4 shadowLightSpace = fineGroundShadow(fineSamples.heights);\n"
        "#else\n  vec3 surfaceNormal = groundFineNormal();\n#endif",
    )
    return {
        "groundFineNormal.glsl": normal,
        "groundFineShadow.glsl": shadow,
        "groundLit.glsl": fragment,
    }

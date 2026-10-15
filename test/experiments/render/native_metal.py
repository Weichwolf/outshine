"""Exercise the production Metal archive: MRT, depth, Hi-Z and compute/readback.

Requires macOS Command Line Tools, glslang and Homebrew spirv-cross. This is
a backend bootstrap, not a complete native renderer or a Place performance test.
"""

import argparse
import hashlib
import json
import os
import platform
import shlex
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def run(command, log=None):
    result = subprocess.run(command, cwd=ROOT, check=False, capture_output=True,
                            text=True, timeout=180)
    if log is not None:
        log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(f"{shlex.join(command)}\n{result.stdout}{result.stderr}")
    return result.stdout


def native_api(output, glslang, toolchain, link_flags):
    shaders = []
    inputs = [ROOT / "src/render/shaders" / name
              for name in ("subjectScanKernel.comp", "subjectCompactKernel.comp")]
    inputs.append(Path(__file__).with_name("native_buffer_lengths.comp"))
    for source in inputs:
        name = source.name
        destination = output / (name + ".spv")
        run([glslang, "-V", "--target-env", "vulkan1.0",
             "-I" + str(ROOT / "src/render/shaders"),
             str(source), "-o", str(destination)])
        shaders.append(str(destination))
    backend = ROOT / "src/render/device/native/metal"
    sources = sorted(backend.glob("*.mm"))
    def build_probe(name, source):
        binary = output / name
        run(["nice", "-n", "10", *shlex.split(os.environ.get("CXX", "c++")), *toolchain,
             "-fno-exceptions",
             *shlex.split(run(["pkg-config", "--cflags", "sdl3"])),
             "-I" + str(ROOT / "src/render/device/native"),
             "-I" + str(ROOT / "test/harness/shared"),
             str(ROOT / "test/harness/render" / source),
             str(ROOT / "build/liboutshine.a"), *link_flags, "-o", str(binary)])
        return binary

    def check_probe(name, source, *arguments):
        binary = build_probe(name, source)
        text = run([str(binary), *map(str, arguments)], output / (name + ".log"))
        verdict = [line.split() for line in text.splitlines() if line.startswith("CHECKS ")]
        expected = ["FAILURES", "0", "SKIPPED", "0", "UNPREPARED", "0", "PARTIAL", "0"]
        if (len(verdict) != 1 or verdict[0][2:] != expected or
                not verdict[0][1].isdigit() or int(verdict[0][1]) <= 0):
            raise SystemExit(f"{name} did not produce a clean verdict: {text}")
        return int(verdict[0][1])

    checks = check_probe("native-compute-probe", "NativeComputeProbe.cpp", *shaders)
    texture_shader = output / "native_texture.comp.spv"
    run([glslang, "-V", "--target-env", "vulkan1.0",
         str(Path(__file__).with_name("native_texture.comp")), "-o", str(texture_shader)])
    texture_checks = check_probe("native-texture-probe", "NativeTextureProbe.cpp",
                                 texture_shader, output / "depthPyramid.comp.spv")
    present = ROOT / "src/render/shaders/present.frag"
    present_shader = output / "present.frag.spv"
    run([glslang, "-V", "--target-env", "vulkan1.0", str(present), "-o", str(present_shader)])
    lifetime_checks = check_probe("native-texture-lifetime-probe", "NativeTextureLifetimeProbe.cpp",
                                   texture_shader, "--validation")
    raster_checks = check_probe("native-raster-probe", "NativeRasterProbe.cpp",
                                output / "fullscreen.vert.spv", output / "native_metal_mrt.frag.spv",
                                present_shader)
    pass_checks = check_probe("native-render-pass-probe", "NativeRenderPassProbe.cpp", shaders[-1])
    draw_shaders = []
    for name in ("native_draw.vert", "native_draw.frag"):
        source = Path(__file__).with_name(name)
        destination = output / (name + ".spv")
        run([glslang, "-V", "--target-env", "vulkan1.0", str(source), "-o", str(destination)])
        draw_shaders.append(str(destination))
    draw_checks = check_probe("native-draw-probe", "NativeDrawProbe.cpp", *draw_shaders)
    mrt_checks = check_probe("native-mrt-probe", "NativeMrtProbe.cpp",
                             output / "fullscreen.vert.spv", output / "native_metal_mrt.frag.spv",
                             output / "depthPyramid.comp.spv")
    indexed_shaders = []
    for name in ("native_indexed.vert", "native_indexed.frag", "native_indexed_arguments.comp"):
        source = Path(__file__).with_name(name)
        destination = output / (name + ".spv")
        run([glslang, "-V", "--target-env", "vulkan1.0", str(source), "-o", str(destination)])
        indexed_shaders.append(destination)
    indexed_checks = check_probe("native-indexed-probe", "NativeIndexedProbe.cpp", *indexed_shaders)
    transition_shaders = []
    for name in ("native_draw_transitions.vert", "native_draw_transitions.frag", "native_draw_indirect.comp"):
        source = Path(__file__).with_name(name)
        destination = output / (name + ".spv")
        run([glslang, "-V", "--target-env", "vulkan1.0", "-Isrc", "-Isrc/render/shaders",
             str(source), "-o", str(destination)])
        transition_shaders.append(destination)
    transition_checks = check_probe("native-draw-transitions-probe", "NativeDrawTransitionsProbe.cpp",
                                    *transition_shaders, "--validation")
    mrt_draw_checks = check_probe("native-mrt-draw-probe", "NativeMrtDrawProbe.cpp",
                                  output / "fullscreen.vert.spv", output / "native_metal_mrt.frag.spv",
                                  "--validation")
    present_shader = output / "native_present.frag.spv"
    run([glslang, "-V", "--target-env", "vulkan1.0",
         str(Path(__file__).with_name("native_present.frag")), "-o", str(present_shader)])
    present_checks = check_probe("native-present-probe", "NativePresentProbe.cpp",
                                 output / "fullscreen.vert.spv", present_shader)
    delay_shader = output / "native_target_delay.comp.spv"
    run([glslang, "-V", "--target-env", "vulkan1.0",
         str(Path(__file__).with_name("native_target_delay.comp")), "-o", str(delay_shader)])
    target_checks = check_probe("native-present-target-probe", "NativePresentTargetProbe.cpp",
                                output / "fullscreen.vert.spv", present_shader, delay_shader)
    return {"checks": checks, "failures": 0, "texture_checks": texture_checks,
            "raster_pipeline_checks": raster_checks, "render_pass_checks": pass_checks,
            "draw_checks": draw_checks, "mrt_depth_pyramid_checks": mrt_checks, "texture_lifetime_checks": lifetime_checks,
            "indexed_indirect_checks": indexed_checks, "draw_transition_checks": transition_checks,
            "mrt_draw_checks": mrt_draw_checks, "presentation_checks": present_checks,
            "presentation_target_checks": target_checks}, sources



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("the direct Metal hardware probe requires macOS")
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        parser.error("private outputs belong in the system temporary directory")
    output.mkdir(parents=True, exist_ok=True)
    glslang = os.environ.get("GLSLANG") or shutil.which("glslangValidator")
    if not glslang:
        glslang = str(ROOT / "build/deps/install/bin/glslangValidator")
    sources = [ROOT / "src/render/shaders/fullscreen.vert",
               Path(__file__).with_name("native_metal_mrt.frag"),
               ROOT / "src/render/shaders/depthPyramid.comp"]
    shaders = []
    for source in sources:
        destination = output / (source.name + ".spv")
        run([glslang, "-V", "--target-env", "vulkan1.0",
             "-I" + str(ROOT / "src/render/shaders"), str(source), "-o", str(destination)])
        shaders.append(str(destination))
    declarations = dict(line.split("=", 1) for line in (ROOT / "test/run.sh").read_text().splitlines()
                        if line.startswith(("CXXSTD=", "WARN=", "OPT=")))
    toolchain = [flag for key in ("CXXSTD", "WARN", "OPT")
                 for flag in shlex.split(declarations[key].strip('"'))]
    link_flags = shlex.split(run(["sh", "test/run.sh", "--native-link-flags"]))
    binary = output / "native-metal-probe"
    run(["nice", "-n", "10", *shlex.split(os.environ.get("CXX", "c++")), *toolchain, "-fno-exceptions",
         "-x", "objective-c++", "-fobjc-arc",
         "-I" + str(ROOT),
         "-I" + str(ROOT / "src/render/device/native"),
         str(ROOT / "test/harness/render/NativeMetalProbe.mm"),
         str(ROOT / "test/harness/render/MetalShader.cpp"),
         "-x", "none", str(ROOT / "build/liboutshine.a"), *link_flags, "-o", str(binary)])
    result = json.loads(run([str(binary), *shaders], output / "native-metal-probe.log"))
    for name, data in (("truncated", b"\x03\x02\x23\x07"), ("bad-header", bytes(20))):
        bad = output / (name + ".spv")
        bad.write_bytes(data)
        rejected = subprocess.run([str(binary), str(bad), *shaders[1:]], check=False, cwd=ROOT,
                                  capture_output=True, text=True, timeout=30)
        if rejected.returncode != 3:
            raise SystemExit(f"{name} was not rejected at the shader boundary: {rejected.stderr}")
    result["invalid_shader_rejections"] = 2
    result["native_compute"], backend_sources = native_api(output, glslang, toolchain, link_flags)
    result["commit"] = run(["git", "rev-parse", "HEAD"]).strip()
    result["working_tree_dirty"] = bool(run(["git", "status", "--porcelain"]))
    files = [*sources, ROOT / "src/render/shaders/fullscreen.glsl",
             ROOT / "test/harness/render/NativeMetalProbe.mm",
             ROOT / "test/harness/render/MetalShader.cpp",
             ROOT / "test/harness/render/MetalShader.h", ROOT / "Makefile", Path(__file__),
             ROOT / "test/run.sh", ROOT / "test/scripts/source_units.py",
             ROOT / "test/harness/render/NativeComputeProbe.cpp",
             ROOT / "test/harness/render/NativeTextureProbe.cpp",
             ROOT / "test/harness/render/NativeTextureLifetimeProbe.cpp",
             ROOT / "test/harness/render/NativeRasterProbe.cpp",
             ROOT / "test/harness/render/NativeRenderPassProbe.cpp",
             ROOT / "test/harness/render/NativeDrawProbe.cpp",
             ROOT / "test/harness/render/NativeIndexedProbe.cpp",
             ROOT / "test/harness/render/NativeDrawTransitionsProbe.cpp",
             ROOT / "test/harness/render/NativeMrtDrawProbe.cpp",
             ROOT / "test/harness/render/NativePresentProbe.cpp",
             ROOT / "test/harness/render/NativePresentTargetProbe.cpp",
             Path(__file__).with_name("native_target_delay.comp"),
             Path(__file__).with_name("native_present.frag"),
             Path(__file__).with_name("native_draw_transitions.vert"),
             Path(__file__).with_name("native_draw_transitions.frag"),
             Path(__file__).with_name("native_draw_indirect.comp"),
             Path(__file__).with_name("native_indexed.vert"),
             Path(__file__).with_name("native_indexed.frag"),
             Path(__file__).with_name("native_indexed_arguments.comp"),
             ROOT / "test/harness/render/NativeMrtProbe.cpp",
             Path(__file__).with_name("native_draw.vert"),
             Path(__file__).with_name("native_draw.frag"),
             ROOT / "src/render/shaders/present.frag",
             ROOT / "test/harness/render/NativeGpuCheck.h",
             Path(__file__).with_name("native_texture.comp"),
             ROOT / "src/render/shaders/subjectScanKernel.comp",
             ROOT / "src/render/shaders/subjectCompactKernel.comp",
             ROOT / "src/render/shaders/cullView.glsl", *backend_sources,
             Path(__file__).with_name("native_buffer_lengths.comp"),
             *(ROOT / "src/render/device/native").glob("*.h"),
             *(ROOT / "src/render/device/native/metal").glob("*.h")]
    result["source_sha256"] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in files}
    archive_hash = hashlib.sha256()
    with (ROOT / "build/liboutshine.a").open("rb") as archive:
        while chunk := archive.read(1024 * 1024):
            archive_hash.update(chunk)
    result["archive_sha256"] = archive_hash.hexdigest()
    result["metal_debug_layer"] = os.environ.get("MTL_DEBUG_LAYER", "")
    result["scope"] = "direct Metal MRT and common-API compute/textures/graphics pipelines/render passes/draws; not native Place acceptance"
    (output / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

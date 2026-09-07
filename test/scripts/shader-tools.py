import json
import pathlib
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[2]
DEPS = ROOT / "build/deps"
PREFIX = DEPS / "install"


def run(*args):
    subprocess.run(args, check=True, cwd=ROOT)


def build(name, folder, options):
    declared = json.loads((ROOT / "test/scripts/shader-toolchain.json").read_text())[name]
    source = DEPS / name
    if not source.exists():
        run("git", "init", str(source))
        run("git", "-C", str(source), "remote", "add", "origin", declared["repository"])
        run("git", "-C", str(source), "fetch", "--depth", "1", "origin", declared["revision"])
        run("git", "-C", str(source), "checkout", "--detach", "FETCH_HEAD")
    revision = subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
    ).strip()
    if revision != declared["revision"]:
        raise SystemExit(f"{source}: expected {declared['revision']}, found {revision}")
    destination = DEPS / folder
    run("cmake", "-S", str(source), "-B", str(destination), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={PREFIX}",
        f"-DCMAKE_INSTALL_RPATH={PREFIX / 'lib'}", *options)
    run("cmake", "--build", str(destination), "-j", "4")
    run("cmake", "--install", str(destination))


if __name__ == "__main__":
    run("pkg-config", "--exists", "sdl3", "spirv-cross-c")
    build("glslang", "glslang-build", ["-DENABLE_OPT=OFF", "-DGLSLANG_TESTS=OFF"])
    build("SDL_shadercross", "shadercross-build",
          ["-DSDLSHADERCROSS_DXC=OFF", "-DSDLSHADERCROSS_SPIRVCROSS_SHARED=OFF"])

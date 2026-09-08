import hashlib
import os
from pathlib import Path
import platform
import shlex
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parents[2]
    os.chdir(root)
    digest = hashlib.sha256()

    def fold(label, content):
        for part in (label.encode(), content):
            digest.update(len(part).to_bytes(8, "little"))
            digest.update(part)

    extensions = {".cpp", ".h", ".inc", ".glsl", ".vert", ".frag", ".comp"}
    files = [p for tree in (Path("src"), Path("include")) for p in tree.rglob("*")
             if p.is_file() and (p.suffix in extensions or p.name == "reaches")]
    files += [Path(p) for p in ("Makefile", "test/run.sh", "test/scripts/crown-provenance.py",
                                "test/scripts/shader-tools.py", "test/scripts/shader-toolchain.json")]
    shaders = sorted(Path("build/shaders").glob("*.spv"))
    if not shaders:
        raise RuntimeError("crown provenance requires built SPIR-V shaders")
    for path in sorted(set(files + shaders)):
        fold(path.as_posix(), path.read_bytes())
    env = os.environ.copy()
    env["PKG_CONFIG_PATH"] = str(root / "build/deps/install/lib/pkgconfig") + ":" + env.get("PKG_CONFIG_PATH", "")
    compiler = shlex.split(env.get("CXX", "c++"))
    fold("compiler", subprocess.check_output(compiler + ["--version"], env=env))
    fold("platform", (platform.system() + "/" + platform.machine()).encode())
    for package in ("sdl3", "sdl3-image", "sdl3-shadercross"):
        fold(package, subprocess.check_output(["pkg-config", "--modversion", package], env=env))
    value = digest.hexdigest()
    target = Path("build/CrownBuild.h")
    content = ('#pragma once\ninline constexpr char kCrownBuildIdentity[] = "' + value + '";\n').encode()
    if target.exists() and target.read_bytes() == content:
        print("crown producer " + value + " (unchanged)")
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as temporary:
        temporary.write(content)
        temporary_path = Path(temporary.name)
    temporary_path.replace(target)
    print("crown producer " + value)


if __name__ == "__main__":
    main()

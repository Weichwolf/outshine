"""Read native diagnostic compiler/linker arguments from CMake's own metadata."""

import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path(os.environ.get("OUTSHINE_BUILD_DIR", ROOT / "build")).resolve()


def metadata():
    reply = BUILD / ".cmake/api/v1/reply"
    indexes = list(reply.glob("index-*.json"))
    if not indexes:
        query = BUILD / ".cmake/api/v1/query/codemodel-v2"
        query.parent.mkdir(parents=True, exist_ok=True)
        query.touch()
        subprocess.run(
            ["cmake", "-S", str(ROOT), "-B", str(BUILD), "-G", "Ninja"],
            check=True,
            stdout=sys.stderr,
        )
        indexes = list(reply.glob("index-*.json"))
    index = json.loads(max(indexes, key=lambda p: p.stat().st_mtime_ns).read_text())
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text())
    return [
        json.loads((reply / target["jsonFile"]).read_text())
        for target in model["configurations"][0]["targets"]
    ]


def compile_flags():
    rows = json.loads((BUILD / "compile_commands.json").read_text())
    row = next(r for r in rows if Path(r["file"]).resolve() == ROOT / "src/client/Main.cpp")
    command = row.get("arguments") or shlex.split(row["command"])
    flags = []
    iterator = iter(command[1:])
    for flag in iterator:
        if flag == "-o":
            next(iterator)
        elif flag != "-c" and flag != row["file"]:
            flags.append(flag)
    return flags


def link_flags(targets):
    internal = {
        (BUILD / artifact["path"]).resolve()
        for target in targets
        for artifact in target.get("artifacts", [])
    }
    executable = next(target for target in targets if target["name"] == "outshine")
    flags = []
    for fragment in executable["link"]["commandFragments"]:
        values = shlex.split(fragment["fragment"])
        if len(values) == 1 and (BUILD / values[0]).resolve() in internal:
            continue
        flags.extend(values)
    return flags


if __name__ == "__main__":
    targets = metadata()
    if sys.argv[1] == "compile":
        print(shlex.join(compile_flags()))
    elif sys.argv[1] == "link":
        print(shlex.join(link_flags(targets)))
    else:
        raise SystemExit("expected compile or link")

"""Complete glslang dependency files with GNU make deleted-header handling."""

from pathlib import Path
import shlex
import sys


def make_path(name):
    result = name.replace("$", "$$")
    for character in ("\\", " ", "#", ":", "%"):
        result = result.replace(character, "\\" + character)
    return result


def write_dependencies(source, destination):
    words = shlex.split(source.read_text().replace("\\\n", ""))
    if len(words) < 2 or not words[0].endswith(":"):
        raise ValueError("glslang emitted no target dependency rule")
    target, dependencies = words[0][:-1], words[1:]
    rule = make_path(target) + ": " + " ".join(map(make_path, dependencies)) + "\n"
    source.write_text(rule + "".join(make_path(name) + ":\n" for name in dependencies))
    source.replace(destination)


if __name__ == "__main__":
    try:
        write_dependencies(Path(sys.argv[1]), Path(sys.argv[2]))
    except (OSError, ValueError, IndexError) as error:
        raise SystemExit(str(error)) from error

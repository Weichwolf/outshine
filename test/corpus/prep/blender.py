"""Locating the pinned Blender and running a script inside it."""

import json
import os
import re
import shutil
import subprocess

from .refusal import Refusal

PROVENANCE_OPEN = "@@outshine-provenance-open@@"
PROVENANCE_CLOSE = "@@outshine-provenance-close@@"

_VERSION = re.compile(r"^Blender (\d+\.\d+\.\d+)")
_BUILD_HASH = re.compile(r"^\s*build hash:\s*([0-9a-f]+)", re.MULTILINE)

_CANDIDATES = (
    "/Applications/Blender.app/Contents/MacOS/Blender",
    "/opt/homebrew/bin/blender",
)


def locate(explicit=None):
    if explicit:
        if not os.path.isfile(explicit) or not os.access(explicit, os.X_OK):
            raise Refusal("blender " + explicit, expected="an executable", observed="not executable")
        return explicit
    from_environment = os.environ.get("OUTSHINE_BLENDER")
    if from_environment:
        return locate(from_environment)
    found = shutil.which("blender")
    if found:
        return found
    for candidate in _CANDIDATES:
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    raise Refusal(
        "blender",
        expected="a Blender on PATH, in OUTSHINE_BLENDER, or at " + " or ".join(_CANDIDATES),
        observed="none",
        why="the oracle is Blender; without it there is no reference to render",
    )


class Blender:
    def __init__(self, executable):
        self.executable = executable
        text = subprocess.run(
            [executable, "--version"], capture_output=True, text=True, check=False
        ).stdout
        first = text.splitlines()[0].strip() if text.strip() else ""
        match = _VERSION.match(first)
        if not match:
            raise Refusal("blender --version", expected="a line 'Blender X.Y.Z'", observed=repr(first))
        self.version = match.group(1)
        build = _BUILD_HASH.search(text)
        self.build_hash = build.group(1) if build else ""

    def against(self, declared):
        """A version difference is recorded, never refused: we do not aim to be bit-identical."""
        if self.version == declared:
            return None
        return "manifest declares Blender %s and this host runs %s" % (declared, self.version)

    def run(self, script, job, blend_file=None):
        command = [self.executable, "--factory-startup", "--background"]
        if blend_file:
            command.append(blend_file)
        command += ["--python-exit-code", "9", "--python", script, "--", job]
        finished = subprocess.run(command, capture_output=True, text=True, check=False)
        if finished.returncode != 0:
            raise Refusal(
                "blender run of " + os.path.basename(script),
                expected="exit status 0",
                observed="exit status %d\n%s\n%s" % (finished.returncode, finished.stdout[-4000:], finished.stderr[-4000:]),
            )
        return _provenance(finished.stdout, script)


def _provenance(stdout, script):
    start = stdout.find(PROVENANCE_OPEN)
    end = stdout.find(PROVENANCE_CLOSE)
    if start < 0 or end < 0 or end < start:
        raise Refusal(
            "blender run of " + os.path.basename(script),
            expected="a delimited provenance block on stdout",
            observed=stdout[-4000:],
            why="a run that does not say what it did cannot be attributed later",
        )
    body = stdout[start + len(PROVENANCE_OPEN) : end]
    try:
        return json.loads(body)
    except ValueError as error:
        raise Refusal("blender provenance", why=str(error), observed=body[:2000])

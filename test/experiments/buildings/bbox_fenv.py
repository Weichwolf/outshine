"""Compare strict Shapely and direct GEOS FP flags without changing modelling behavior."""

import argparse
import ctypes as ct
import hashlib
import json
import os
import platform
import shlex
import subprocess
import sys
import tempfile
import warnings
from pathlib import Path

import numpy as np
import shapely
from evidence import revision

UNARY = ct.CFUNCTYPE(ct.c_void_p, ct.c_void_p, ct.c_void_p)
NAMES = ("all", "divide_by_zero", "invalid", "overflow", "underflow", "inexact")
CASES = ("square", "rotated-rectangle", "line", "point")


def bind(library, name, arguments, result):
    function = getattr(library, name)
    function.argtypes, function.restype = arguments, result
    return function


class FloatEnvironment:
    def __init__(self, path):
        self.library = ct.CDLL(str(path))
        self.mask = bind(self.library, "outshine_fenv_mask", [ct.c_int], ct.c_int)
        self.flags = bind(self.library, "outshine_fenv_flags", [], ct.c_int)
        self.rounding = bind(self.library, "outshine_fenv_rounding", [], ct.c_int)
        self.control = bind(self.library, "outshine_fenv_control", [], ct.c_int)
        self.geometry = bind(
            self.library,
            "outshine_fenv_geometry",
            [UNARY, ct.c_void_p, ct.c_void_p, ct.POINTER(ct.c_void_p)],
            ct.c_int,
        )
        self.masks = {name: self.mask(index) for index, name in enumerate(NAMES)}

    def decoded(self, flags):
        if flags < 0:
            raise ValueError("Direct FENV measurement failed: " + str(flags))
        return {
            name: bool(flags & value)
            for name, value in self.masks.items()
            if name != "all"
        }


class DirectGeos:
    def __init__(self, path=None):
        selected = (
            str(path.resolve(strict=True)) if path is not None else shapely.lib.__file__
        )
        self.path = selected
        self.library = ct.CDLL(selected)
        self.version = bind(self.library, "GEOSversion", [], ct.c_char_p)().decode(
            "ascii"
        )
        pointer, size = ct.c_void_p, ct.c_size_t
        self.init = bind(self.library, "GEOS_init_r", [], pointer)
        self.finish = bind(self.library, "GEOS_finish_r", [pointer], None)
        self.reader_new = bind(
            self.library, "GEOSWKBReader_create_r", [pointer], pointer
        )
        self.reader_free = bind(
            self.library, "GEOSWKBReader_destroy_r", [pointer, pointer], None
        )
        self.read = bind(
            self.library,
            "GEOSWKBReader_read_r",
            [pointer, pointer, ct.c_char_p, size],
            pointer,
        )
        self.writer_new = bind(
            self.library, "GEOSWKBWriter_create_r", [pointer], pointer
        )
        self.writer_free = bind(
            self.library, "GEOSWKBWriter_destroy_r", [pointer, pointer], None
        )
        self.write = bind(
            self.library,
            "GEOSWKBWriter_write_r",
            [pointer, pointer, pointer, ct.POINTER(size)],
            pointer,
        )
        self.free = bind(self.library, "GEOSFree_r", [pointer, pointer], None)
        self.destroy = bind(
            self.library, "GEOSGeom_destroy_r", [pointer, pointer], None
        )
        self.operation = ct.cast(self.library.GEOSMinimumRotatedRectangle_r, UNARY)

    def measure(self, encoded, fenv):
        context = self.init()
        if not context:
            raise ValueError("GEOS context allocation failed")
        reader = writer = geometry = result = output = None
        try:
            reader, writer = self.reader_new(context), self.writer_new(context)
            if not reader or not writer:
                raise ValueError("GEOS WKB reader/writer allocation failed")
            geometry = self.read(context, reader, encoded, len(encoded))
            if not geometry:
                raise ValueError("GEOS rejected the valid control WKB")
            result = ct.c_void_p()
            flags = fenv.geometry(self.operation, context, geometry, ct.byref(result))
            decoded = fenv.decoded(flags)
            if not result.value:
                raise ValueError("GEOS minimum rectangle returned no geometry")
            size = ct.c_size_t()
            output = self.write(context, writer, result, ct.byref(size))
            if not output or not size.value:
                raise ValueError("GEOS failed to encode its actual rectangle")
            return ct.string_at(output, size.value), decoded
        finally:
            if output:
                self.free(context, output)
            if result and result.value:
                self.destroy(context, result)
            if geometry:
                self.destroy(context, geometry)
            if writer:
                self.writer_free(context, writer)
            if reader:
                self.reader_free(context, reader)
            self.finish(context)


def shape(name):
    if name == "square":
        return shapely.box(0, 0, 1, 1)
    wkt = {
        "rotated-rectangle": "POLYGON ((0 0, 6.4 4.8, 5.2 6.4, -1.2 1.6, 0 0))",
        "line": "LINESTRING (0 0, 5 0)",
        "point": "POINT (3 4)",
    }
    return shapely.from_wkt(wkt[name])


def probe(name, helper, geos_library=None):
    fenv = FloatEnvironment(helper)
    initial_flags, initial_rounding = fenv.flags(), fenv.rounding()
    control = fenv.decoded(fenv.control())
    if not control["divide_by_zero"] or not control["invalid"]:
        raise ValueError("FENV observer failed its native exception-flag control")
    if fenv.flags() != initial_flags or fenv.rounding() != initial_rounding:
        raise ValueError(
            "FENV control did not restore its caller's complete environment"
        )
    source = shape(name)
    rounding = fenv.rounding()
    before = fenv.decoded(fenv.flags())
    strict, error = None, None
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("error", RuntimeWarning)
            strict = shapely.oriented_envelope(source)
    except RuntimeWarning as exception:
        error = str(exception)
    after = fenv.decoded(fenv.flags())
    backend = DirectGeos(geos_library)
    actual, direct_flags = backend.measure(shapely.to_wkb(source), fenv)
    direct = shapely.from_wkb(actual)
    equal = (
        None
        if strict is None
        else bool(shapely.equals_exact(strict.normalize(), direct.normalize(), 0.0))
    )
    return {
        "case": name,
        "rounding": rounding,
        "masks": fenv.masks,
        "native_flag_control": control,
        "native_control_preserved_environment": True,
        "before_wrapper_flags": before,
        "after_wrapper_flags": after,
        "strict_wrapper_error": error,
        "direct_geos_flags": direct_flags,
        "direct_geos_library": backend.path,
        "direct_geos_version": backend.version,
        "direct_geometry": shapely.to_wkt(direct, rounding_precision=17),
        "strict_geometry": None
        if strict is None
        else shapely.to_wkt(strict, rounding_precision=17),
        "exact_normalized_geometry_equal": equal,
        "numpy_error_policy": np.geterr(),
    }


def compile_helper(output):
    source = Path(__file__).with_suffix(".c")
    suffix = ".dylib" if sys.platform == "darwin" else ".so"
    library = output / ("bbox-fenv" + suffix)
    command = [
        *shlex.split(os.environ.get("CC", "cc")),
        "-std=c11",
        "-O0",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-frounding-math",
        "-fno-fast-math",
        "-dynamiclib" if sys.platform == "darwin" else "-shared",
        str(source),
        "-lm",
        "-o",
        str(library),
    ]
    log = Path(tempfile.gettempdir()) / (
        "outshine-bbox-fenv-"
        + hashlib.sha256(source.read_bytes()).hexdigest()[:12]
        + ".log"
    )
    with log.open("w") as stream:
        subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True)
    return library, {
        "command": command,
        "log": str(log),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    }


def run(output, geos_library=None):
    output.mkdir(parents=True, exist_ok=True)
    library, compilation = compile_helper(output)
    records = []
    for name in CASES:
        result = subprocess.run(
            [
                sys.executable,
                "-W",
                "error",
                str(Path(__file__).resolve()),
                "--case",
                name,
                "--helper",
                str(library.resolve()),
                *(
                    ["--geos-library", str(geos_library.resolve(strict=True))]
                    if geos_library
                    else []
                ),
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        records.append(json.loads(result.stdout))
    report = revision() | {
        "status": "diagnostic-recorded",
        "cases": records,
        "compilation": compilation,
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "numpy": np.__version__,
            "shapely": shapely.__version__,
            "geos": shapely.geos_version_string,
            "extension": shapely.lib.__file__,
            "direct_geos_library": str(geos_library.resolve(strict=True))
            if geos_library
            else shapely.lib.__file__,
        },
        "scope": "Fresh process per case; strict Python warnings retained, direct GEOS flags measured in C with its own context and WKB ownership. No modelling/backend change, warning suppression, platform fix or native gate claim.",
    }
    (output / "bbox-fenv.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "status": report["status"],
                "output": str(output / "bbox-fenv.json"),
                "cases": len(records),
                "strict_wrapper_errors": sum(
                    r["strict_wrapper_error"] is not None for r in records
                ),
                "direct_fp_error_cases": sum(
                    any(
                        flag
                        for name, flag in r["direct_geos_flags"].items()
                        if name != "inexact"
                    )
                    for r in records
                ),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/buildings/bbox-fenv")
    )
    parser.add_argument("--case", choices=CASES)
    parser.add_argument("--helper", type=Path)
    parser.add_argument("--geos-library", type=Path)
    args = parser.parse_args()
    if args.case:
        if not args.helper:
            parser.error("--case requires --helper")
        print(json.dumps(probe(args.case, args.helper, args.geos_library)))
    else:
        run(args.output, args.geos_library)

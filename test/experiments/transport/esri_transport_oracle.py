"""Isolated pinned SDK operations after header/assembly/canary ABI verification."""

import argparse
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np

from esri_transport_catalog import BINARY_SHA256, CORE


ROOT = Path(__file__).resolve().parents[3]
OFFSET = "_ZN11CoreToolbox10PolygonOps13computeOffsetEfPKfm"
EDGES = "_ZN4util12intersection14edgesIntersectERKNS_7Vector3IdEES4_S4_S4_RS2_"
RAY = "_ZN4util12intersection9getXPointERKNS_7Vector3IdEES4_S4_S4_iRS2_Rd"
ADDRESS = {OFFSET: 0x45b090, EDGES: 0x473c00, RAY: 0x473800}
GUARD = 0x7819ACD93257DE08


class GuardedPoint(c.Structure):
    _fields_ = [("before", c.c_uint64), ("point", c.c_double * 3), ("after", c.c_uint64)]


class VectorHeader(c.Structure):
    _fields_ = [("start", c.c_void_p), ("end", c.c_void_p), ("capacity", c.c_void_p)]


class GuardedScalar(c.Structure):
    _fields_ = [("before", c.c_uint64), ("value", c.c_double), ("after", c.c_uint64)]


class Version(c.Structure):
    _fields_ = [("major", c.c_int), ("minor", c.c_int), ("build", c.c_int)]
    _fields_ += [(name, c.c_void_p) for name in ("name", "full_name", "version", "config", "os", "arch", "toolchain", "date")]
    _fields_ += [("wide_" + name, c.c_void_p) for name in ("name", "full_name", "version", "config", "os", "arch", "toolchain", "date")]
    _fields_ += [("cga_major", c.c_int), ("cga_minor", c.c_int), ("cga_version", c.c_void_p), ("wide_cga_version", c.c_void_p),
                 ("cgac_major", c.c_int), ("cgac_minor", c.c_int), ("cgac_version", c.c_void_p), ("wide_cgac_version", c.c_void_p)]


def readable(address, size):
    if not address or size <= 0:
        raise ValueError("invalid SDK memory range")
    cursor, end = address, address + size
    for line in Path("/proc/self/maps").read_text().splitlines():
        fields = line.split()
        lo, hi = (int(value, 16) for value in fields[0].split("-"))
        if lo <= cursor < hi and fields[1].startswith("r"):
            cursor = min(hi, end)
            if cursor == end:
                return
    raise ValueError("SDK result is not completely mapped readable memory")


def loader():
    if sys.byteorder != "little" or c.sizeof(c.c_void_p) != 8 or hashlib.sha256(CORE.read_bytes()).hexdigest() != BINARY_SHA256:
        raise ValueError("operation ABI requires pinned little-endian x86-64 SDK")
    library = c.CDLL(str(CORE))
    get_version = getattr(library, "_ZN3prt10getVersionEv")
    get_version.argtypes, get_version.restype = [], c.c_void_p
    pointer = get_version()
    if c.sizeof(Version) != 192 or Version.version.offset != 32:
        raise ValueError("public Version header layout mismatch")
    readable(pointer, c.sizeof(Version))
    version = Version.from_address(pointer)
    if (version.major, version.minor, version.build) != (3, 4, 12206):
        raise ValueError("public SDK version differs from requested binary")
    readable(version.version, 16)
    if c.string_at(version.version, 16).split(b"\0")[0] != b"3.4.12206":
        raise ValueError("SDK version text and fields disagree")
    anchor = c.cast(getattr(library, OFFSET), c.c_void_p).value - ADDRESS[OFFSET]
    return library, anchor


def intersections(library, base, request):
    if not isinstance(request.get("vectors"), list) or not 1 <= len(request["vectors"]) <= 4096:
        raise ValueError("intersection query cap exhausted before numeric conversion")
    values = np.asarray(request["vectors"], dtype=np.float64)
    operation = request["operation"]
    if values.ndim != 3 or values.shape[1:] != (4, 3) or not 1 <= len(values) <= 4096 or not np.isfinite(values).all():
        raise ValueError("bounded finite four-Vector3 cases required")
    if operation == "ray" and request.get("mode") not in (0, 1, 2):
        raise ValueError("only explicitly investigated ray modes allowed")
    args = [c.c_void_p] * 4 + ([c.c_int, c.c_void_p, c.c_void_p] if operation == "ray" else [c.c_void_p])
    name = RAY if operation == "ray" else EDGES
    function = c.CFUNCTYPE(c.c_bool, *args)(base + ADDRESS[name])
    records, elapsed = [], []
    for vectors in values:
        vectors = np.ascontiguousarray(vectors)
        point = GuardedPoint(GUARD, (c.c_double * 3)(np.nan, np.nan, np.nan), GUARD)
        distance = GuardedScalar(GUARD, np.nan, GUARD)
        parameters = [vectors[index].ctypes.data for index in range(4)]
        output = c.byref(point, GuardedPoint.point.offset)
        parameters += [request["mode"], output, c.byref(distance, GuardedScalar.value.offset)] if operation == "ray" else [output]
        began = time.perf_counter_ns()
        accepted = bool(function(*parameters))
        elapsed.append(time.perf_counter_ns() - began)
        if point.before != GUARD or point.after != GUARD or distance.before != GUARD or distance.after != GUARD:
            raise ValueError("Vector3 output overwrote ABI canaries")
        candidate = np.asarray(point.point)
        if accepted and (not np.isfinite(candidate).all() or operation == "ray" and not np.isfinite(distance.value)):
            raise ValueError("SDK accepted a nonfinite contact")
        records.append(dict(accepted=accepted, candidate_xyz=candidate.tolist() if np.isfinite(candidate).all() else None,
                            parameter=distance.value if operation == "ray" and np.isfinite(distance.value) else None))
    return dict(results=records, kernel_call_ns=elapsed, symbol=name, elf_address=hex(ADDRESS[name]), output_canaries_verified=True)


def offset_sdk(library, vertices, distance):
    function = getattr(library, OFFSET)
    function.argtypes, function.restype = [c.c_float, c.c_void_p, c.c_size_t], c.c_void_p
    began = time.perf_counter_ns()
    pointer = function(distance, vertices.ctypes.data, len(vertices))
    kernel_ns = time.perf_counter_ns() - began
    readable(pointer, 24)
    header = VectorHeader.from_address(pointer)
    start, end, capacity = header.start or 0, header.end or 0, header.capacity or 0
    length = end - start
    if not 0 <= start <= end <= capacity or length % 12 or length > 4096 * 12:
        raise ValueError("SDK offset vector exceeds its bounded ABI")
    if length:
        readable(start, length)
        result = np.frombuffer(c.string_at(start, length), dtype=np.float32).copy().reshape(-1, 3)
    else:
        result = np.empty((0, 3), dtype=np.float32)
    if not np.isfinite(result).all():
        raise ValueError("SDK offset returned nonfinite geometry")
    return result[:, [0, 2]].tolist(), kernel_ns, time.perf_counter_ns() - began


def offset(library, request):
    import shapely
    if not isinstance(request.get("xy"), list) or not 3 <= len(request["xy"]) <= 256:
        raise ValueError("offset ring cap exhausted before numeric conversion")
    xy = np.asarray(request["xy"], dtype=np.float32)
    distance = float(np.float32(request["distance"]))
    if xy.ndim != 2 or xy.shape[1] != 2 or not 3 <= len(xy) <= 256 or not np.isfinite(xy).all() or not 0 < distance < 1e6:
        raise ValueError("bounded finite single ring and positive inward offset required")
    polygon = shapely.Polygon(xy)
    if not polygon.is_valid or polygon.is_empty:
        raise ValueError("SDK helper requires a simple quantized ring")
    xy = np.asarray(shapely.orient_polygons(polygon).exterior.coords, dtype=np.float32)[:-1]
    vertices = np.ascontiguousarray(np.c_[xy[:, 0], np.zeros(len(xy)), xy[:, 1]], dtype=np.float32)
    backend = request.get("backend", "both")
    if backend not in ("sdk", "python", "both"):
        raise ValueError("known isolated offset backend required")
    records, sdk_ns, own_ns, kernel_ns = [], [], [], []
    for _ in range(request.get("repetitions", 1)):
        result, own_rings = None, None
        if backend != "python":
            result, kernel, elapsed = offset_sdk(library, vertices, distance)
            sdk_ns.append(elapsed)
            kernel_ns.append(kernel)
        if backend != "sdk":
            began = time.perf_counter_ns()
            own = shapely.buffer(polygon, -distance, join_style="mitre", mitre_limit=1e6)
            components = shapely.get_parts(own)
            own_rings = [shapely.get_coordinates(part).tolist() for part in components if not part.is_empty]
            own_ns.append(time.perf_counter_ns() - began)
        records.append(dict(sdk_vertices_xz=result, own_rings_xy=own_rings))
    return dict(results=records, sdk_operation_and_decode_ns=sdk_ns, python_operation_and_decode_ns=own_ns,
                sdk_kernel_call_ns=kernel_ns, symbol=OFFSET, elf_address=hex(ADDRESS[OFFSET]), input_quantum="float32 XY and distance for both methods",
                ownership="SDK heap result retained only until bounded child exit",
                scope="CoreToolbox single ring helper; positive inward offset, no hole/multipart argument; same mitre operation, not full CGA/round roadbed semantics")


def worker(request):
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_CPU, (8, 8))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 2**30, 2 * 2**30))
    operation = request.get("operation")
    library, base = (None, None) if operation == "offset" and request.get("backend") == "python" else loader()
    if operation in ("edges", "ray"):
        result = intersections(library, base, request)
    elif operation == "offset":
        repeats = request.get("repetitions", 1)
        if type(repeats) is not int or not 1 <= repeats <= 25:
            raise ValueError("bounded repetition count required before native work")
        result = offset(library, request)
    elif operation == "version":
        result = dict(verified_version=[3, 4, 12206], public_version_bytes=c.sizeof(Version))
    elif operation == "tessellation":
        from esri_tessellation_reference import triangulate
        result = triangulate(request)
    elif operation == "pause-test":
        c.CDLL(None).pause()
        raise AssertionError("pause must be killed by parent timeout")
    elif operation == "memory-test":
        import mmap
        try:
            allocation = mmap.mmap(-1, 3 * 2**30)
        except OSError as error:
            if error.errno != 12:
                raise
            result = dict(address_space_limit_verified=True, rejected_allocation_bytes=3 * 2**30)
        else:
            allocation.close()
            raise AssertionError("allocation bypassed configured address-space limit")
    else:
        raise ValueError("operation outside verified ABI")
    result["operation_scope"] = result.get("scope", "Scalar Vector3 calls with guarded output; ABI says nothing about higher-level transport semantics")
    status = Path("/proc/self/status").read_text().splitlines()
    peak = next(int(line.split()[1]) * 1024 for line in status if line.startswith("VmHWM:"))
    result.update(binary_sha256=BINARY_SHA256, sdk_version=[3, 4, 12206], cpu_limit_s=8, address_space_limit_bytes=2 * 2**30,
                  sdk_dependency_loaded=library is not None,
                  peak_rss_bytes=peak, peak_rss_source="Linux /proc/self/status VmHWM: current exec address space, includes imports/preparation/receipt checks",
                  fork_exec_rusage_peak_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
                  scope="Isolated external operation, not CGA Processor/network/runtime equivalence; child exit frees SDK allocations")
    return result


def reference(request, *, wall_limit_s=10.):
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(CORE.parent) + os.pathsep + env.get("LD_LIBRARY_PATH", "")
    env["OPENBLAS_NUM_THREADS"] = env["OMP_NUM_THREADS"] = "1"
    began = time.perf_counter()
    child = subprocess.run([sys.executable, "-W", "error", str(Path(__file__).resolve()), "--worker"],
                           input=json.dumps(request), capture_output=True, text=True, env=env, timeout=wall_limit_s)
    if child.returncode:
        raise RuntimeError(f"bounded SDK child exited {child.returncode}: " + child.stderr[-700:])
    result = json.loads(child.stdout)
    result["whole_child_ms"] = (time.perf_counter() - began) * 1000
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(json.load(sys.stdin))))
    else:
        print(json.dumps(reference(dict(operation="version"))))

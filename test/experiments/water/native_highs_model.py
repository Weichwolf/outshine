"""Write standard MPS through the installed HiGHS C API; no solver runs here."""

import ctypes as ct
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy import sparse


def write_model(path, matrix, cost, lower, upper, row_upper, hessian=None, offset=0):
    directory = subprocess.check_output(
        ["pkg-config", "--variable=libdir", "highs"], text=True
    ).strip()
    library = ct.CDLL(
        str(
            Path(directory)
            / ("libhighs.dylib" if sys.platform == "darwin" else "libhighs.so")
        )
    )
    library.Highs_create.restype = ct.c_void_p
    library.Highs_destroy.argtypes = [ct.c_void_p]
    library.Highs_getSizeofHighsInt.argtypes = [ct.c_void_p]
    model = library.Highs_create()
    if not model:
        raise RuntimeError("HiGHS model allocation failed")
    try:
        size = library.Highs_getSizeofHighsInt(model)
        if size not in (4, 8):
            raise RuntimeError("unsupported HiGHS integer ABI")
        integer = ct.c_int32 if size == 4 else ct.c_int64
        ip, dp = ct.POINTER(integer), ct.POINTER(ct.c_double)
        library.Highs_setBoolOptionValue.argtypes = [ct.c_void_p, ct.c_char_p, integer]
        library.Highs_passLp.argtypes = (
            [ct.c_void_p] + [integer] * 5 + [ct.c_double] + [dp] * 5 + [ip] * 2 + [dp]
        )
        library.Highs_passHessian.argtypes = (
            [ct.c_void_p] + [integer] * 3 + [ip] * 2 + [dp]
        )
        library.Highs_writeModel.argtypes = [ct.c_void_p, ct.c_char_p]
        for name in ("Highs_passColName", "Highs_passRowName"):
            getattr(library, name).argtypes = [ct.c_void_p, integer, ct.c_char_p]

        def checked(status):
            if status != 0:
                raise RuntimeError(f"HiGHS model export status {status}")

        def doubles(values):
            return np.ascontiguousarray(values, dtype=np.float64)

        def integers(values):
            values = np.asarray(values)
            if values.size and (
                values.min() < 0 or values.max() > np.iinfo(integer).max
            ):
                raise ValueError("sparse index exceeds installed HiGHS integer ABI")
            return np.ascontiguousarray(
                values, dtype=np.int32 if size == 4 else np.int64
            )

        checked(library.Highs_setBoolOptionValue(model, b"output_flag", 0))
        csr = matrix.tocsr()
        start, index = integers(csr.indptr), integers(csr.indices)
        arrays = [
            doubles(x)
            for x in (
                cost,
                lower,
                upper,
                np.full(csr.shape[0], -np.inf),
                row_upper,
                csr.data,
            )
        ]
        pointers = [a.ctypes.data_as(dp) for a in arrays]
        checked(
            library.Highs_passLp(
                model,
                csr.shape[1],
                csr.shape[0],
                csr.nnz,
                2,
                1,
                offset,
                *pointers[:5],
                start.ctypes.data_as(ip),
                index.ctypes.data_as(ip),
                pointers[5],
            )
        )
        for name, count, prefix in (
            ("Highs_passColName", csr.shape[1], "v"),
            ("Highs_passRowName", csr.shape[0], "r"),
        ):
            for at in range(count):
                checked(getattr(library, name)(model, at, f"{prefix}{at}".encode()))
        if hessian is not None:
            n = csr.shape[1]
            triangle = sparse.tril(hessian, format="csc")
            starts, indices, values = (
                integers(triangle.indptr),
                integers(triangle.indices),
                doubles(triangle.data),
            )
            checked(
                library.Highs_passHessian(
                    model,
                    n,
                    triangle.nnz,
                    1,
                    starts.ctypes.data_as(ip),
                    indices.ctypes.data_as(ip),
                    values.ctypes.data_as(dp),
                )
            )
        checked(library.Highs_writeModel(model, str(path).encode()))
    finally:
        library.Highs_destroy(model)

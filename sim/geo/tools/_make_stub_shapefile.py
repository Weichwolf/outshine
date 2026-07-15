"""Produce a minimal, empty ESRI Shapefile zip as a stand-in for the
Shortbread admin_points source (its upstream URL returns 404 as of 2026).

Output: tools/.cache/stubs/admin_points.zip, containing
  admin-points-4326.shp/.shx/.dbf/.prj -- all header-only, no records.

That is enough for Planetiler's shapefile reader to open the dataset and
iterate zero features, which is the expected outcome for point-class
admin labels inside a tiny bbox like Hameln anyway.
"""
import argparse
import os
import struct
import sys
import zipfile


def shapefile_header(shape_type: int) -> bytes:
    h  = struct.pack(">i", 9994)        # file code
    h += b"\x00" * 20                    # unused 5x int32
    h += struct.pack(">i", 50)           # file length in 16-bit words (header only)
    h += struct.pack("<ii", 1000, shape_type)
    h += struct.pack("<dddd", 0.0, 0.0, 0.0, 0.0)  # bbox xy
    h += struct.pack("<dddd", 0.0, 0.0, 0.0, 0.0)  # bbox zm
    return h


def empty_dbf() -> bytes:
    # dBase III with one CHAR field, zero records. Keeps GDAL happy.
    header_size = 32 + 32 + 1           # main header + 1 field desc + 0x0D terminator
    record_size = 1 + 50                # deletion flag + 'name' C(50)
    dbf  = struct.pack("<BBBB", 3, 26, 1, 1)   # type, YY, MM, DD
    dbf += struct.pack("<I", 0)                 # record count
    dbf += struct.pack("<HH", header_size, record_size)
    dbf += b"\x00" * 20                         # reserved
    dbf += b"name".ljust(11, b"\x00")          # field name
    dbf += b"C"                                 # field type: Character
    dbf += b"\x00" * 4                         # field data address (unused)
    dbf += struct.pack("<B", 50)                # field length
    dbf += struct.pack("<B", 0)                 # decimal count
    dbf += b"\x00" * 14                        # reserved
    dbf += b"\x0d"                              # header terminator
    dbf += b"\x1a"                              # EOF marker (no records)
    return dbf


PRJ_WGS84 = (
    'GEOGCS["WGS 84",DATUM["WGS_1984",'
    'SPHEROID["WGS 84",6378137,298.257223563]],'
    'PRIMEM["Greenwich",0],'
    'UNIT["degree",0.0174532925199433]]'
)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("out_zip")
    ap.add_argument("--basename", default="admin-points-4326")
    ap.add_argument("--shape-type", type=int, default=1)  # 1 = Point
    args = ap.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(args.out_zip)), exist_ok=True)
    hdr = shapefile_header(args.shape_type)
    dbf = empty_dbf()

    with zipfile.ZipFile(args.out_zip, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(args.basename + ".shp", hdr)
        z.writestr(args.basename + ".shx", hdr)
        z.writestr(args.basename + ".dbf", dbf)
        z.writestr(args.basename + ".prj", PRJ_WGS84)

    print(f"wrote {args.out_zip}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

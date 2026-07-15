"""Re-pack a PMTiles v3 archive with internal_compression=none.

osmmesh's T2 reader only accepts internal_compression=1 (none) to stay
free of zlib. Planetiler and go-pmtiles both hard-code internal_compression
= gzip when they write the root/leaf directories + metadata blob, so we
need a post-processing pass that rewrites those sections uncompressed.

Tile payloads themselves (and tile_compression) are passed through.

Usage:
    python _pmtiles_decompress.py INPUT.pmtiles OUTPUT.pmtiles
"""
import argparse
import sys

from pmtiles import reader as _r
from pmtiles import writer as _w
from pmtiles.reader import MmapSource, all_tiles, zxy_to_tileid
from pmtiles.writer import Writer


def _identity(data: bytes) -> bytes:
    return data


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    args = ap.parse_args()

    # 1) Read the input header + metadata while upstream gzip logic still
    #    operates normally.
    with open(args.input, "rb") as f_in:
        get_bytes = MmapSource(f_in)  # pmtiles' MmapSource is the closure
        reader = _r.Reader(get_bytes)
        header = dict(reader.header())
        metadata = reader.metadata()

        # 2) Switch the writer's gzip.compress to a no-op so the serialized
        #    root / leaf directories + metadata blob are stored verbatim.
        _w.gzip.compress = _identity

        with open(args.output, "wb") as f_out:
            wr = Writer(f_out)
            for zxy, tile_bytes in all_tiles(get_bytes):
                z, x, y = zxy
                wr.write_tile(zxy_to_tileid(z, x, y), tile_bytes)
            wr.finalize(header, metadata)

    # 3) Writer.finalize hard-codes internal_compression=GZIP in the
    #    header bytes. Patch that single byte to NONE (=1).
    from pmtiles.reader import deserialize_header
    from pmtiles.tile import Compression
    from pmtiles.writer import serialize_header
    with open(args.output, "rb+") as f:
        head = f.read(127)
        parsed = deserialize_header(head)
        # deserialize_header returns ints, serialize_header expects enums.
        parsed["internal_compression"] = Compression.NONE
        parsed["tile_compression"] = Compression(parsed["tile_compression"]) \
            if isinstance(parsed["tile_compression"], int) \
            else parsed["tile_compression"]
        new_head = serialize_header(parsed)
        if len(new_head) != 127:
            raise SystemExit(f"serialize_header returned {len(new_head)} bytes, expected 127")
        f.seek(0)
        f.write(new_head)

    print(
        f"repacked {args.input} -> {args.output} "
        f"with internal_compression=none"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

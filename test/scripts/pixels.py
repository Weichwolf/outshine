#!/usr/bin/env python3
"""How far two shots differ, pixel by pixel.

A DIGEST ANSWERS ONE QUESTION AND IT IS THE WRONG ONE DURING A REBUILD. `make shots` hashes each
picture, which catches an unintended change perfectly and says nothing at all about a change that
was intended: a structural rewrite moves every hash, and the question then is not WHETHER the
picture moved but by HOW MUCH. This answers that -- how many pixels differ, and by how much of 255.

Reads PNG without a library, because the tree has no image dependency and does not want one for a
measuring tool.
"""

import pathlib
import struct
import sys
import zlib


def read(path):
    data = pathlib.Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")
    at = 8
    wide = high = channels = 0
    body = b""
    while at + 8 <= len(data):
        size = struct.unpack(">I", data[at : at + 4])[0]
        kind = data[at + 4 : at + 8]
        chunk = data[at + 8 : at + 8 + size]
        if kind == b"IHDR":
            wide, high, depth, colour = struct.unpack(">IIBB", chunk[:10])
            if depth != 8:
                raise ValueError(f"{path} is {depth} bits a channel, and this reads 8")
            channels = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
        elif kind == b"IDAT":
            body += chunk
        at += 12 + size
    raw = zlib.decompress(body)
    stride = wide * channels
    out = bytearray(high * stride)
    prior = bytearray(stride)
    pos = 0
    for row in range(high):
        how = raw[pos]
        pos += 1
        line = bytearray(raw[pos : pos + stride])
        pos += stride
        if how == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif how == 2:
            for i in range(stride):
                line[i] = (line[i] + prior[i]) & 0xFF
        elif how == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + prior[i]) >> 1)) & 0xFF
        elif how == 4:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                up = prior[i]
                corner = prior[i - channels] if i >= channels else 0
                guess = left + up - corner
                dl, du, dc = abs(guess - left), abs(guess - up), abs(guess - corner)
                line[i] = (line[i] + (left if dl <= du and dl <= dc else up if du <= dc else corner)) & 0xFF
        out[row * stride : (row + 1) * stride] = line
        prior = line
    return wide, high, channels, bytes(out)


def apart(one, two):
    wideA, highA, chA, a = read(one)
    wideB, highB, chB, b = read(two)
    if (wideA, highA, chA) != (wideB, highB, chB):
        return None
    pixels = wideA * highA
    moved = 0
    worst = 0
    over1 = 0
    box = [wideA, highA, -1, -1]
    rows = {}
    loudest = []
    for at in range(0, len(a), chA):
        gap = max(abs(a[at + k] - b[at + k]) for k in range(min(3, chA)))
        if gap:
            moved += 1
            worst = max(worst, gap)
            if gap > 1:
                over1 += 1
            x, y = (at // chA) % wideA, (at // chA) // wideA
            box = [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
            rows[y] = rows.get(y, 0) + 1
            loudest.append((gap, x, y, tuple(a[at : at + 3]), tuple(b[at : at + 3])))
    loudest.sort(reverse=True)
    return {
        "Pixels": pixels,
        "Moved": moved,
        "Worst": worst,
        "OverOne": over1,
        "Box": box,
        "Rows": len(rows),
        "Loudest": loudest[:6],
    }


def main(argv):
    if len(argv) != 2:
        print("pixels.py <before.png> <after.png>", file=sys.stderr)
        return 2
    told = apart(argv[0], argv[1])
    if told is None:
        print("the two pictures are not the same size, so there is nothing to compare")
        return 1
    share = 100.0 * told["Moved"] / told["Pixels"]
    print(
        f"{told['Moved']} of {told['Pixels']} pixel(s) differ ({share:.4f} %), "
        f"{told['OverOne']} by more than 1 of 255, worst {told['Worst']} of 255"
    )
    if told["Moved"]:
        x0, y0, x1, y1 = told["Box"]
        print(f"  WHERE x {x0}..{x1} y {y0}..{y1}, over {told['Rows']} row(s)")
        for gap, x, y, before, after in told["Loudest"]:
            print(f"  {gap:3d} at ({x:4d},{y:4d}) before {before} after {after}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

"""Compare finite-depth reductions and logical reads; this does not measure GPU time."""
import json
import random


def reduce_blocks(image, block):
    height, width = len(image), len(image[0])
    return [[min(image[min(y + dy, height - 1)][min(x + dx, width - 1)]
                 for dy in range(block) for dx in range(block))
             for x in range(0, width, block)] for y in range(0, height, block)]


def direct(image):
    return [reduce_blocks(image, 2 << level) for level in range(4)]


def chained(image):
    levels = []
    for _ in range(4):
        image = reduce_blocks(image, 2)
        levels.append(image)
    return levels


def tiled(image):
    height, width = len(image), len(image[0])
    levels = [[[None] * ((width + (2 << level) - 1) // (2 << level))
               for _ in range((height + (2 << level) - 1) // (2 << level))]
              for level in range(4)]
    for y in range(0, height, 16):
        for x in range(0, width, 16):
            tile = [[image[min(y + dy, height - 1)][min(x + dx, width - 1)]
                     for dx in range(16)] for dy in range(16)]
            for level, reduced in enumerate(chained(tile)):
                block = 2 << level
                for row, values in enumerate(reduced):
                    for col, value in enumerate(values):
                        dest_y, dest_x = y // block + row, x // block + col
                        if dest_y < len(levels[level]) and dest_x < len(levels[level][0]):
                            levels[level][dest_y][dest_x] = value
    return levels


def logical_reads(width, height):
    raw = sum(((width + block - 1) // block) * ((height + block - 1) // block) * block**2
              for block in (2, 4, 8, 16))
    hierarchy = 0
    for _ in range(4):
        width, height = (width + 1) // 2, (height + 1) // 2
        hierarchy += 4 * width * height
    return raw, hierarchy


def main():
    randomizer = random.Random(282)
    cases = 0
    for width, height in ((1, 1), (1, 17), (17, 1), (16, 16), (17, 15), (2, 3), (31, 17), (65, 33)):
        for mode in range(3):
            image = [[randomizer.random() if mode == 0 else float(mode - 1)
                      for _ in range(width)] for _ in range(height)]
            image[-1][-1] = 0.0
            reference = direct(image)
            assert chained(image) == reference
            assert tiled(image) == reference
            cases += 1
    raw, hierarchy = logical_reads(1280, 720)
    print(json.dumps(dict(equal_cases=cases, profile=[1280, 720], direct_fetches=raw,
                          chained_reads=hierarchy, tiled_texture_fetches=1280 * 720,
                          tiled_shared_floats=64 + 16 + 4 + 1, dispatches=dict(direct=4, tiled=1),
                          measurement="logical reads, not DRAM bytes or GPU time")))


if __name__ == '__main__':
    main()

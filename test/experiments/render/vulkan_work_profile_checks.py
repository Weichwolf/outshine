"""Exercise incomplete, undefined and corrupted Vulkan diagnostic records."""

import copy
import tempfile
from pathlib import Path

from vulkan_work_analysis import ABSENT, analyze, interval
from vulkan_work_profile import instrument, link_command


def fixture():
    records = []
    for serial in range(1, 61):
        records.append(f"VK_WORK_FRAME\t{serial}\t3\t0\t1000\t1\tfalse")
        records.append(f"VK_WORK_SAMPLE\t{serial}\t0\t0\t{ABSENT}\t1\tsky.frag.spv\tfalse\ttrue\ttrue\ttrue\t100\t900\t1\t64\t3\t1\t3\t16\t0\t0\t0\t0")
        records.append(f"VK_WORK_SAMPLE\t{serial}\t1\t0\t{ABSENT}\t0\tclear\tfalse\ttrue\ttrue\tfalse\t10\t20\t1\t64\t0\t0\t0\t0\t0\t0\t0\t0")
        records.append(f"VK_WORK_SAMPLE\t{serial}\t2\t2\t{ABSENT}\t1\tcompute\tfalse\ttrue\ttrue\tfalse\t20\t80\t1\t64\t0\t0\t0\t0\t64\t1\t1\t1")
        records.append(f"VK_WORK_COMPUTE\t{serial}\t0\ttrue\t20\t80\t1\t64")
    records.append("VK_WORK_COST\t100\t1000\t1000\t60\t0\t0\t34952")
    row = ["ROW", "Synthetic", "00000000", "1", "0", "0", "0", "60", "60", "0", "0", "0", "0", "0", "0", "0", "0", "0", "360", "-"]
    records.append("\t".join(row))
    records.append("GPUROW\tSynthetic\t60\t60\t0.06\t0.001\t0\t0")
    return records


def change(lines, tag, column, value):
    index = next(i for i, line in enumerate(lines) if line.startswith(tag + "\t"))
    fields = lines[index].split("\t")
    fields[column] = value
    lines[index] = "\t".join(fields)


def main():
    checks = 0
    with tempfile.TemporaryDirectory(prefix="outshine-vulkan-work-checks-", dir="/tmp") as name:
        path = Path(name) / "capture.log"
        records = fixture()
        path.write_text("\n".join(records))
        report = analyze(path)
        assert report["sky_frame_count"] == 60
        assert interval(250, 5, 8, 2) == 22
        assert all(x["fragment_invocations"] is None for x in report["samples"] if not x["fragment_present"])
        assert all(x["vertex_invocations"] is None for x in report["samples"] if x["kind"] == 2 or x["calls"] == 0)
        checks += 4
        corruptions = [
            ("VK_WORK_FRAME", 3, "1"),
            ("VK_WORK_FRAME", 5, "2"),
            ("VK_WORK_COST", 5, "1"),
            ("VK_WORK_SAMPLE", 7, "true"),
            ("VK_WORK_SAMPLE", 8, "false"),
            ("VK_WORK_SAMPLE", 9, "false"),
            ("VK_WORK_SAMPLE", 4, "999"),
            ("VK_WORK_SAMPLE", 10, "invalid"),
            ("VK_WORK_SAMPLE", 15, "-1"),
            ("ROW", 7, "59"),
            ("GPUROW", 4, "1"),
            ("VK_WORK_COMPUTE", 3, "false"),
        ]
        for tag, column, value in corruptions:
            changed = copy.copy(records)
            change(changed, tag, column, value)
            path.write_text("\n".join(changed))
            try:
                analyze(path)
            except ValueError:
                checks += 1
            else:
                raise AssertionError((tag, column, value))
        for tag in ("VK_WORK_FRAME", "VK_WORK_SAMPLE", "ROW", "GPUROW"):
            path.write_text("\n".join(records + [next(x for x in records if x.startswith(tag + "\t"))]))
            try:
                analyze(path)
            except ValueError:
                checks += 1
            else:
                raise AssertionError(f"duplicate {tag}")
        for source in ("", "    EncodeDraw(draw);\n    EncodeDraw(draw);"):
            try:
                instrument("RenderPassEncoding", source, False)
            except ValueError:
                checks += 1
            else:
                raise AssertionError("ambiguous native source accepted")
        try:
            link_command(path, Path(name))
        except ValueError:
            checks += 1
        else:
            raise AssertionError("invalid client link accepted")
    print(f"{checks}/{checks} Vulkan diagnostic controls PASS")


if __name__ == "__main__":
    main()

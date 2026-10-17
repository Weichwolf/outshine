"""Run the native raw-extinction-moment consumer against a matching engine archive."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from cloud_moment_cases import ANSWER_VECTORS, cases, verify

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_shader(source, target, run, name, includes=()):
    run(
        [
            "nice",
            "-n",
            "10",
            *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
            "-V",
            *(f"-I{path}" for path in includes),
            "-Isrc/generators/weather/shaders",
            "-Itest/experiments/weather",
            source,
            "-o",
            target,
        ],
        name,
    )


def mutations(output, binary, queries, labels, run):
    kernel = (ROOT / "test/experiments/weather/cloud_moments.comp").read_text()
    helper = (ROOT / "src/generators/weather/shaders/cloudMoments.glsl").read_text()
    variants = {
        "midpoint": (
            kernel,
            helper.replace("lengthM * cloudMomentCentroid(optical)", "lengthM * 0.5"),
        ),
        "naive-opacity": (
            kernel,
            helper.replace(
                "float cloudMomentOpacity(float optical) {",
                "float cloudMomentOpacity(float optical) {\n  return 1.0 - exp(-optical);",
            ),
        ),
        "receiver-partition": (
            kernel.replace(
                "query.DirectionExtentM.w,",
                "min(query.DirectionExtentM.w, query.OriginQueryM.w),",
                1,
            ),
            helper,
        ),
    }
    result = {}
    for name, (source, include) in variants.items():
        if source == kernel and include == helper:
            raise RuntimeError(
                f"the {name} negative control did not change its intended input"
            )
        folder = output / name
        folder.mkdir(exist_ok=True)
        (folder / "probe.comp").write_text(source)
        (folder / "cloudMoments.glsl").write_text(include)
        shader = folder / "probe.spv"
        compile_shader(folder / "probe.comp", shader, run, name + "-compile", (folder,))
        run(
            [binary, shader, output / "queries.bin", folder / "answers.bin"],
            name + "-native",
        )
        answers = np.fromfile(folder / "answers.bin", dtype="<f4").reshape(
            -1, ANSWER_VECTORS, 4
        )
        try:
            verify(queries, answers, labels)
        except AssertionError as error:
            result[name] = str(error).splitlines()[0]
        else:
            raise RuntimeError(f"wrong native {name} implementation was not rejected")
    return result


def reflected_layout(shader, run):
    data = json.loads(
        run(
            [os.environ.get("SPIRV_CROSS", "spirv-cross"), shader, "--reflect"],
            "reflection",
        )
    )
    expected = {"Queries": (0, 608), "Answers": (1, 528)}
    for resource in data["ssbos"]:
        slot, stride = expected.pop(resource["name"])
        member = data["types"][resource["type"]]["members"][0]
        if (resource["set"], resource["binding"], member["array_stride"]) != (
            slot,
            0,
            stride,
        ):
            raise RuntimeError(
                "SPIR-V input/output stride differs from the C++ static contract"
            )
        record = data["types"][member["type"]]
        offsets = [field["offset"] for field in record["members"]]
        if offsets != ([0, 48, 64, 80, 96] if slot == 0 else [0, 16]):
            raise RuntimeError(
                "SPIR-V member offsets differ from the C++ static contract"
            )
        if slot == 0:
            layer = data["types"][record["members"][0]["type"]]
            if [field["offset"] for field in layer["members"]] != [0, 16, 32]:
                raise RuntimeError(
                    "SPIR-V cloud layer differs from its three native vectors"
                )
    if expected:
        raise RuntimeError("the actual native shader lost an input/output block")


def build_probe(api, output, run):
    archive = api / "build/liboutshine.a"
    commit = run(["git", "rev-parse", "HEAD"], "api-commit", api).strip()
    if run(["git", "status", "--porcelain", "--", "src", "include"], "api-status", api):
        raise RuntimeError("the archive API checkout has modified source/headers")
    if run(
        [
            "git",
            "diff",
            commit,
            "--name-only",
            "--",
            "src/render/device/native",
            "include",
        ],
        "api-match",
    ):
        raise RuntimeError(
            "consumer native implementation/public headers differ from the archive API producer"
        )
    declaration = dict(
        line.split("=", 1)
        for line in (ROOT / "test/run.sh").read_text().splitlines()
        if line.startswith(("CXXSTD=", "WARN=", "OPT="))
    )
    flags = [
        flag
        for key in ("CXXSTD", "WARN", "OPT")
        for flag in shlex.split(declaration[key].strip('"'))
    ]
    linkage = shlex.split(
        run(["sh", api / "test/run.sh", "--native-link-flags"], "link-flags", api)
    )
    binary = output / "cloud-moments-native"
    includes = (
        api / "include",
        api / "src",
        api / "src/render/device/native",
        ROOT / "test/harness/shared",
        ROOT / "test/harness/render",
    )
    run(
        [
            "nice",
            "-n",
            "10",
            *shlex.split(os.environ.get("CXX", "c++")),
            *flags,
            "-fno-exceptions",
            *(f"-I{path}" for path in includes),
            ROOT / "test/experiments/weather/cloud_moments_native.cpp",
            archive,
            *linkage,
            "-o",
            binary,
        ],
        "build-probe",
    )
    return binary, {
        "commit": commit,
        "archive_sha256": digest(archive),
        "consumer_sha256": digest(binary),
    }


def plot(output, answers, expected, labels):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages

    selected = np.array(
        [
            label["kind"] == "cell" and label["begin_m"] == 0 and label["optical"] > 0
            for label in labels
        ]
    )
    optical = np.array([label.get("optical", 0) for label in labels])[selected]
    actual = answers[selected, 0, :2]
    reference = expected[selected]
    with PdfPages(output / "cloud-moments.pdf") as pdf:
        fig, axes = plt.subplots(1, 2, figsize=(12, 4.5), constrained_layout=True)
        for axis, channel, title in zip(
            axes, (0, 1), ("M0", "M1 (metres)"), strict=True
        ):
            axis.loglog(
                optical, reference[:, channel], label="independent Decimal integral"
            )
            axis.loglog(optical, actual[:, channel], ".", label="native GPU raw value")
            axis.set(xlabel="cell optical depth", ylabel=title)
            axis.legend()
        fig.suptitle("Actual GPU extinction moments: thin through saturated cells")
        pdf.savefig(fig)
        plt.close(fig)
        fig, axes = plt.subplots(1, 2, figsize=(12, 4.5), constrained_layout=True)
        relative = np.abs(actual - reference) / reference
        for channel, name in enumerate(("M0", "M1")):
            axes[0].semilogx(optical, relative[:, channel], ".", label=name)
        axes[0].axhline(3e-6, color="red", label="fixed FP32 test limit")
        axes[0].set(xlabel="cell optical depth", ylabel="relative error")
        axes[0].legend()
        ids = [i for i, label in enumerate(labels) if label["kind"] == "two-depths"]
        limits = [labels[i]["limit_m"] for i in ids]
        visible = answers[ids, 0, 0] > 0
        depths = np.divide(
            answers[ids, 0, 1],
            answers[ids, 0, 0],
            out=np.zeros(len(ids)),
            where=visible,
        )
        axes[1].plot(limits, depths, "o-")
        axes[1].set(xlabel="receiver distance (m)", ylabel="finite M1/M0 (m)")
        fig.suptitle(
            "Exact constant-cell integration; one depth is still a multilayer approximation"
        )
        pdf.savefig(fig)
        plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in the system temporary directory")
    output.mkdir(parents=True, exist_ok=True)
    receipt = output / "receipt.json"
    receipt.unlink(missing_ok=True)
    record = {"commands": []}

    def run(argv, name, cwd=ROOT):
        argv = list(map(str, argv))
        result = subprocess.run(
            argv, cwd=cwd, capture_output=True, text=True, timeout=180, check=False
        )
        (output / f"{name}.stdout").write_text(result.stdout)
        (output / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {"argv": argv, "cwd": str(cwd), "exit_code": result.returncode}
        )
        if result.returncode or result.stderr:
            raise RuntimeError(
                f"{name} failed or emitted diagnostics; inspect {output}"
            )
        return result.stdout

    queries, labels = cases()
    queries.tofile(output / "queries.bin")
    (output / "queries.json").write_text(json.dumps(labels, indent=2))
    binary, producer = build_probe(args.archive_root.resolve(), output, run)
    shader = output / "cloud-moments.spv"
    compile_shader(
        "test/experiments/weather/cloud_moments.comp", shader, run, "compile-shader"
    )
    reflected_layout(shader, run)
    run([binary, shader, output / "queries.bin", output / "answers.bin"], "native")
    answers = np.fromfile(output / "answers.bin", dtype="<f4").reshape(
        -1, ANSWER_VECTORS, 4
    )
    report, expected = verify(queries, answers, labels)
    record["rejected_native_mutations"] = mutations(
        output, binary, queries, labels, run
    )
    plot(output, answers, expected, labels)
    record.update(
        {
            "commit": run(["git", "rev-parse", "HEAD"], "commit").strip(),
            "producer": producer,
            "shader_sha256": digest(shader),
            "queries_sha256": digest(output / "queries.bin"),
            "answers_sha256": digest(output / "answers.bin"),
            "result": report,
        }
    )
    receipt.write_text(json.dumps(record, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

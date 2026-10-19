"""Build a private Vulkan work probe from the actual native client build commands."""

import argparse
import hashlib
import json
import shlex
import shutil
import subprocess
from pathlib import Path
from time import monotonic

from vulkan_work_analysis import analyze

ROOT = Path(__file__).resolve().parents[3]
HEADER = ROOT / "test/harness/render/VulkanWorkProfile.h"
UNITS = ("Context", "GraphicsPipeline", "RenderPassEncoding", "Recording", "Device", "ComputeRecording")


def replace(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f"missing or ambiguous native anchor: {old[:100]}")
    return source.replace(old, new)


def instrument(name, source, validation):
    if name == "Context":
        anchor = "  core.depthClamp = features.features.depthClamp;"
        source = replace(source, anchor, anchor + '\n  WorkProbe::Require(features.features.pipelineStatisticsQuery != VK_FALSE, "pipeline statistics unsupported");\n  core.pipelineStatisticsQuery = VK_TRUE;')
        anchor = "  if (Logical != VK_NULL_HANDLE) { vkDestroyDevice(Logical, nullptr); }"
        source = replace(source, anchor, "  if (Logical != VK_NULL_HANDLE) { WorkProbe::Clear(Logical); vkDestroyDevice(Logical, nullptr); }")
        if validation:
            anchor = "Result<void> InitInstance(Context &context, DeviceOptions options) {"
            source = replace(source, anchor, anchor + "\n  options.Validation = true;")
    elif name == "GraphicsPipeline":
        anchor = '    return std::unexpected(Failure("vkCreateGraphicsPipelines", result));\n  }\n  return {};'
        source = replace(source, anchor, '    return std::unexpected(Failure("vkCreateGraphicsPipelines", result));\n  }\n  WorkProbe::Register(state.Pipeline->Handle, device, description.Label, description.Fragment.has_value());\n  return {};')
    elif name == "RenderPassEncoding":
        anchor = "  vkCmdBeginRendering(Commands_, &rendering);"
        source = replace(source, anchor, "  const auto workPass = WorkProbe::Begin(*Context_, Commands_, pass.Draws);\n" + anchor)
        anchor = "    EncodeDraw(draw);"
        source = replace(source, anchor, "    const auto workDraw = WorkProbe::BeginDraw(Commands_, workPass, draw);\n" + anchor + "\n    WorkProbe::End(Commands_, workDraw);")
        anchor = "  vkCmdEndRendering(Commands_);"
        source = replace(source, anchor, anchor + "\n  WorkProbe::End(Commands_, workPass);")
    elif name == "Recording":
        anchor = "  assert(OnOwner(*Context_));"
        source = replace(source, anchor, anchor + "\n  if (Context_->GpuTiming) {\n    const WorkProbe::CpuScope workCost;\n    WorkProbe::Finish(*Context_, Commands_, Submitted_, Submitted_ ? Timings_.ReadSubmission() : Result<SubmissionGpuTiming>{SubmissionGpuTiming{}}, Submitted_ ? Timings_.Read() : GpuTimingResults{});\n  }")
    elif name == "Device":
        anchor = "    return CreatePipeline(Context_, spirv, options.Entry);"
        source = replace(source, anchor, '    auto result = CreatePipeline(Context_, spirv, options.Entry);\n    if (result) {\n      const auto *native = dynamic_cast<const VulkanPipeline *>(result->get());\n      WorkProbe::Require(native != nullptr, "non-Vulkan compute pipeline");\n      WorkProbe::Register(native->State()->Handle, Context_->Logical, options.Label, false);\n    }\n    return result;')
    elif name == "ComputeRecording":
        anchor = "  vkCmdDispatch(Commands_, groups.X, groups.Y, groups.Z);"
        source = replace(source, anchor, "  const auto workCompute = WorkProbe::BeginCompute(*Context_, Commands_, native->State()->Handle, groups);\n" + anchor + "\n  WorkProbe::End(Commands_, workCompute);")
    else:
        raise ValueError(f"unrecognized native unit: {name}")
    return '#include ' + json.dumps(str(HEADER)) + "\n" + source


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(command, root, log, timeout=None):
    start = monotonic()
    with log.open("w") as stream:
        try:
            result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT, check=False, timeout=timeout)
        except subprocess.TimeoutExpired:
            return {"command": command, "exit": 124, "seconds": monotonic() - start, "timeout": True}
    return {"command": command, "exit": result.returncode, "seconds": monotonic() - start}


def link_command(path, root):
    command = json.loads(path.read_text())
    if not isinstance(command, list) or not command or not all(isinstance(x, str) for x in command):
        raise ValueError("link-command requires a JSON array of actual client-link arguments")
    archive = root / "build/liboutshine.a"
    matches = [i for i, value in enumerate(command) if not value.startswith("-") and (root / value).resolve() == archive]
    if len(matches) != 1 or command.count("-o") != 1 or "-c" in command:
        raise ValueError("client link requires exactly one archive and one output")
    if any(value == "-flto" or value.startswith("-flto=") for value in command):
        raise ValueError("LTO interposition is unsupported")
    return command


def build(root, output, validation, link_path):
    db = json.loads((root / "compile_commands.json").read_text())
    receipt = {"runtime_root": str(root), "runtime_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(), "header_sha256": sha(HEADER), "sources_sha256": {}, "commands": [], "validation": validation, "exit": None}
    receipt["link_input_sha256"] = sha(link_path)
    receipt["tool_files_sha256"] = {str(path): sha(path) for path in (Path(__file__), Path(__file__).with_name("vulkan_work_analysis.py"), HEADER)}
    receipt["runtime_status"] = subprocess.check_output(["git", "status", "--porcelain"], cwd=root, text=True)
    objects = []
    for name in UNITS:
        source = root / f"src/render/device/native/vulkan/{name}.cpp"
        private = output / f"{name}.probe.cpp"
        private.write_text(instrument(name, source.read_text(), validation))
        receipt["sources_sha256"][str(source)] = sha(source)
        receipt["sources_sha256"][str(private)] = sha(private)
        entry = next(x for x in db if Path(x["file"]) == source)
        args = list(entry.get("arguments") or shlex.split(entry["command"]))
        obj = output / f"{name}.probe.o"
        args[args.index(str(source))] = str(private)
        args[args.index("-o") + 1] = str(obj)
        objects.append(str(obj))
        result = run(args, root, output / f"{name}.log")
        receipt["commands"].append(result)
        (output / "build.json").write_text(json.dumps(receipt, indent=2))
        if result["exit"]:
            raise RuntimeError(f"{name} failed: {result['exit']}")
    args = link_command(link_path, root)
    archive = root / "build/liboutshine.a"
    index = next(i for i, x in enumerate(args) if not x.startswith("-") and (root / x).resolve() == archive)
    args[index:index] = objects
    binary = output / "outshine-work"
    args[args.index("-o") + 1] = str(binary)
    receipt["archive_sha256"] = sha(archive)
    receipt["client_sha256"] = sha(root / "build/outshine")
    result = run(args, root, output / "link.log")
    receipt["commands"].append(result)
    receipt["exit"] = result["exit"]
    if not result["exit"]:
        receipt["binary_sha256"] = sha(binary)
    (output / "build.json").write_text(json.dumps(receipt, indent=2))
    if result["exit"]:
        raise RuntimeError(f"private client link failed: {result['exit']}")
    return binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--validation", action="store_true")
    parser.add_argument("--place")
    parser.add_argument("--link-command", type=Path, required=True)
    parser.add_argument("--overrides", default='{"render":{"widthPx":1920,"heightPx":1080,"fps":60}}')
    args = parser.parse_args()
    root = args.runtime_root.resolve()
    output = args.output.resolve()
    if not output.is_relative_to(Path("/tmp")) or output == Path("/tmp"):
        parser.error("private outputs require a dedicated /tmp directory")
    output.mkdir(parents=True, exist_ok=False)
    binary = build(root, output, args.validation, args.link_command.resolve())
    if args.place:
        command = [str(binary), "shots", "--stats", "--rows", "--measures", "--offline", "--preload-seconds", "120", "--scenario-overrides", args.overrides, args.place]
        result = run(command, root, output / "client.log", timeout=180)
        (output / "run.json").write_text(json.dumps(result, indent=2))
        if result["exit"]:
            raise RuntimeError(f"diagnostic client failed: {result['exit']}")
        report = analyze(output / "client.log", expected_frames=json.loads(args.overrides)["render"]["fps"])
        image = root / "build/shots/places" / f"{args.place}-{report['ROW'][2]}.png"
        shutil.copy2(image, output / "image.png")
        report["image_sha256"] = sha(image)
        (output / "work.json").write_text(json.dumps(report, indent=2))
    print(output)


if __name__ == "__main__":
    main()

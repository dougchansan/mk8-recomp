"""Build and verify public synthetic native tools; never package game inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime, timezone


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="New package directory")
    parser.add_argument("--vulkan-include", type=Path, required=True)
    parser.add_argument("--sirit-root", type=Path, required=True)
    parser.add_argument("--sirit-license", type=Path, required=True)
    parser.add_argument("--sirit-source-archive", type=Path, required=True)
    args = parser.parse_args()
    directory = Path(__file__).resolve().parent
    project = directory.parent if directory.name == "scripts" else directory
    source, build, output = args.source.resolve(), args.build.resolve(), args.output.resolve()
    manifest_path = project / "native-checkpoint/source.json"
    if not manifest_path.exists():
        manifest_path = project / "SOURCE.json"
    manifest = json.loads(manifest_path.read_text())
    for name, expected in manifest["files_sha256"].items():
        if sha(source / name) != expected:
            raise RuntimeError(f"Source differs from checkpoint: {name}")
    if output.exists():
        raise RuntimeError("Package output must be a new directory")
    if os.name != "nt":
        raise RuntimeError("This checkpoint packages the verified Windows x64 targets")
    vulkan, sirit = args.vulkan_include.resolve(), args.sirit_root.resolve()
    for dependency in (vulkan / "vulkan/vulkan.h", sirit / "lib/sirit-static.lib",
                       args.sirit_license, args.sirit_source_archive):
        if not dependency.is_file():
            raise RuntimeError(f"Missing existing dependency: {dependency}")
    build.mkdir(parents=True, exist_ok=True)
    records = []

    def run(label, command, cwd=None):
        command = list(map(str, command))
        print(f"{label} ...", flush=True)
        log = build / f"{label}.log"
        with log.open("w", encoding="utf-8") as stream:
            result = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                    creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
        if result.returncode:
            raise RuntimeError(f"{label} failed ({result.returncode}); see {log}")
        replacements = {str(source): "<source>", str(build): "<build>",
                        str(vulkan): "<vulkan-include>", str(sirit): "<sirit-root>"}
        public_command = []
        for word in command:
            for local, portable in replacements.items():
                word = word.replace(local, portable)
            public_command.append(word)
        records.append({"step": label, "command": public_command, "exit_code": 0,
                        "log_sha256": sha(log)})

    def executable(folder, name):
        for path in (folder / "Release" / f"{name}.exe", folder / f"{name}.exe"):
            if path.is_file():
                return path
        raise FileNotFoundError(name)

    run("platform", [sys.executable, source / "tests/native_platform/run.py", "--build", build / "platform"])
    for suite in ("native_entry",):
        run(suite, [sys.executable, source / f"tests/{suite}/run.py", "--build", build / suite])
    run("native_rsqrt-configure", ["cmake", "-S", source / "tests/native_rsqrt", "-B", build / "native_rsqrt",
                                    f"-DSUYU_SOURCE={source}"])
    run("native_rsqrt-build", ["cmake", "--build", build / "native_rsqrt", "--config", "Release", "--parallel", "2"])
    run("native_rsqrt-correctness", [executable(build / "native_rsqrt", "rsqrt_test")])
    run("native_api", [sys.executable, source / "tests/native_api/run.py", "--build", build / "native_api",
                       "--vulkan-include", vulkan])
    run("buffer_init", [executable(build / "native_api/test", "buffer_init_test")])
    for suite in ("native_nvn", "native_nvn_builders"):
        run(f"{suite}-configure", ["cmake", "-S", source / f"tests/{suite}", "-B", build / suite])
        run(f"{suite}-build", ["cmake", "--build", build / suite, "--config", "Release", "--parallel", "2"])
        run(f"{suite}-test", ["ctest", "--test-dir", build / suite, "-C", "Release", "--output-on-failure"])
    for component in ("services", "graphics", "nvn"):
        command = ["cmake", "-S", source / f"native-runtime/{component}", "-B", build / component,
                   f"-DNATIVE_GRAPHICS_VULKAN_INCLUDE={vulkan}", f"-DNATIVE_GRAPHICS_SIRIT_ROOT={sirit}"]
        run(f"{component}-configure", command)
        run(f"{component}-build", ["cmake", "--build", build / component, "--config", "Release", "--parallel", "2"])
        run(f"{component}-test", ["ctest", "--test-dir", build / component, "-C", "Release", "--output-on-failure"])
    tools = {
        "platform": ("native_emit", "native_admit", "native_platform_test", "native_primitive_test",
                     "native_loader_test", "native_ipc_test"),
        "graphics": ("native_graphics_probe", "native_graphics_resource_test"),
        "nvn": ("native_nvn_resource_test", "native_nvn_draw_test", "native_nvn_pipeline_check"),
        "services": ("native_services_test",),
        "native_api/test": ("buffer_init_test",),
    }
    output.mkdir(parents=True)
    binary = output / "bin"
    binary.mkdir()
    for folder, names in tools.items():
        for name in names:
            shutil.copy2(executable(build / folder, name), binary / f"{name}.exe")
    # These are created by the checked-in synthetic fixture generator, not copied from a title.
    shutil.copytree(build / "platform/fixture", output / "fixtures/platform")
    shutil.copy2(source / "LICENSE.txt", output / "COPYING.txt")
    shutil.copy2(args.sirit_license, output / "Sirit-LICENSE.txt")
    shutil.copy2(args.sirit_source_archive, output / "Sirit-v1.0.5-source.tar.gz")
    shutil.copytree(sirit / "include", output / "dependencies/sirit/include")
    shutil.copytree(vulkan, output / "dependencies/vulkan/include")
    for notice in (vulkan.parent / "LICENSES").glob("*.txt"):
        destination = output / "dependencies/vulkan/LICENSES" / notice.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(notice, destination)
    run("base-source-archive", ["git", "-C", source, "archive", "--format=tar.gz",
                               f"--output={output / 'suyu-base-source.tar.gz'}", manifest["base_commit"]])
    shutil.copy2(manifest_path, output / "SOURCE.json")
    patch = project / "src/patches/native-runtime.patch"
    if not patch.exists():
        patch = project / "native-runtime.patch"
    shutil.copy2(patch, output / "native-runtime.patch")
    readme = project / "docs/native-builds.md"
    if not readme.exists():
        readme = project / "README.md"
    shutil.copy2(readme, output / "README.md")
    shutil.copy2(directory / "prepare-native-source.py", output / "prepare-native-source.py")
    shutil.copy2(Path(__file__), output / "package-native-checkpoint.py")
    # Assert every checkpoint source file still matches after all builds/tests.
    for name, expected in manifest["files_sha256"].items():
        if sha(source / name) != expected:
            raise RuntimeError(f"Source changed during packaging: {name}")
    proof = {
        "schema": 1, "built_at_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "Native foundations and synthetic verification tools; not a playable MK8D build",
        "base_commit": manifest["base_commit"], "source_patch_sha256": manifest["patch_sha256"],
        "game_code_or_assets_included": False, "full_native_gameplay": False,
        "verification": records,
        "dependencies": {"Vulkan-Headers": {"version": "v1.4.345", "header_sha256": sha(vulkan / "vulkan/vulkan.h")},
                         "Sirit": {"version": "v1.0.5", "static_library_sha256": sha(sirit / "lib/sirit-static.lib"),
                                   "source_archive_sha256": sha(args.sirit_source_archive),
                                   "source": "https://github.com/eden-emulator/sirit/tree/v1.0.5"}},
        "files_sha256": {str(p.relative_to(output)).replace("\\", "/"): sha(p)
                         for p in sorted(output.rglob("*")) if p.is_file()},
    }
    (output / "BUILD.json").write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    archive = shutil.make_archive(str(output), "zip", output)
    print(json.dumps({"archive": archive, "sha256": sha(Path(archive)), "tools": len(list(binary.glob('*.exe')))}))


if __name__ == "__main__":
    main()

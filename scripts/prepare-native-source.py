"""Apply the reviewed native checkpoint to a new, pinned suyu worktree."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess


def run(*args, cwd=None):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, help="Existing suyu Git checkout")
    parser.add_argument("--output", type=Path, help="New worktree directory")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    directory = Path(__file__).resolve().parent
    project = directory.parent if directory.name == "scripts" else directory
    manifest_path = project / "native-checkpoint/source.json"
    if not manifest_path.exists():
        manifest_path = project / "SOURCE.json"
    manifest = json.loads(manifest_path.read_text())
    patch = project / "src/patches/native-runtime.patch"
    if not patch.exists():
        patch = project / "native-runtime.patch"
    if hashlib.sha256(patch.read_bytes()).hexdigest() != manifest["patch_sha256"]:
        raise RuntimeError("Checkpoint patch hash does not match source.json")
    for name in manifest["files_sha256"]:
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts:
            raise RuntimeError("Invalid checkpoint source path")
    if args.verify_only:
        print(f"Verified native checkpoint patch: {manifest['patch_sha256']}")
        return
    if args.source is None or args.output is None:
        parser.error("--source and --output are required without --verify-only")
    source, output = args.source.resolve(), args.output.resolve()
    if output.exists() or source.is_relative_to(output) or output.is_relative_to(source):
        raise RuntimeError("Output must be a new directory outside the source checkout")
    top = Path(run("git", "rev-parse", "--show-toplevel", cwd=source).stdout.strip())
    if top.resolve() != source:
        raise RuntimeError("--source must name the suyu checkout root")
    base = manifest["base_commit"]
    run("git", "cat-file", "-e", f"{base}^{{commit}}", cwd=source)
    output.parent.mkdir(parents=True, exist_ok=True)
    run("git", "-c", "core.autocrlf=false", "worktree", "add", "--detach", str(output), base, cwd=source)
    run("git", "-c", "core.autocrlf=false", "apply", "--check", str(patch), cwd=output)
    run("git", "-c", "core.autocrlf=false", "apply", str(patch), cwd=output)
    for name, expected in manifest["files_sha256"].items():
        if hashlib.sha256((output / name).read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"Applied source hash mismatch: {name}")
    print(f"Prepared verified native source at {output}")
    print("Build instructions: docs/native-builds.md")


if __name__ == "__main__":
    main()

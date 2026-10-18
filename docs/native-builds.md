# Native development builds

There is currently no native MK8D release or download. All five native-foundations
prereleases, their download assets and their tags have been removed at the project
owner's request. Do not publish another interim foundations checkpoint.

## Release gate

A release tag and downloadable build must wait until the complete native game is
working. Verification must cover static CPU execution, live game-driven native
GPU rendering and presentation, platform services, audio, controls, saves and
restart through the supported gameplay path, without CPU JIT, Maxwell/emulated
GPU processing or the general emulator runtime. Component fixtures and captured
background replays do not satisfy this gate. Record the tested scope and remaining
limitations before publication.

The current campaign has not met this gate. See
[native runtime progress](native-runtime-progress.md) for evidence and open work.

## Local development and retained evidence

Continue building and testing locally. Keep development executables, diagnostic
bundles, hashes and review receipts in the gitignored `local/native-builds`
directory. Existing local builds remain available for regression checks; retaining
a local archive does not make it a published release.

The historical generic source snapshot is retained in
`src/patches/native-runtime.patch`, with its source manifest in
`native-checkpoint/source.json` and historical verification in
`native-checkpoint/verification.json`. It targets suyu commit
`347fcf91d95cd39cb5c3db50f1b6d827ad699311` and predates current private integration
work. These records document earlier component verification; they do not identify
an available download or establish native gameplay.

To prepare that source for local inspection, use Python 3.10+, Git, CMake 3.22+
and Visual Studio C/C++ build tools. Source preparation requires an unused output
directory and checks file hashes:

```powershell
git clone --no-checkout https://github.com/dougchansan/suyu-v0.0.4 suyu-source
git -C suyu-source fetch origin 347fcf91d95cd39cb5c3db50f1b6d827ad699311
python scripts/prepare-native-source.py --source ./suyu-source --output ./native-source
```

The existing `scripts/package-native-checkpoint.py` helper produces local component
archives only; running it does not satisfy the release gate or authorize an upload.
The repository workflow runs emitter tests and does not publish releases.

Game-linked diagnostics require matching locally supplied title inputs. Keep title
code, generated game archives, assets, shaders, captures, keys and firmware out of
public commits and download assets. See [LEGAL.md](../LEGAL.md).

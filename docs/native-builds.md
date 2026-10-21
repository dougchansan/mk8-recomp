# Native development builds

There is currently no native MK8D release or download. All five native-foundations
prereleases, their download assets and their tags have been removed at the project
owner's request. Do not publish another interim foundations checkpoint.

## Public release policy and local completion gate

**Public game-linked binaries are prohibited.** Even after gameplay is complete,
no game-linked executable, DLL, shader cache, mechanically translated code or
Nintendo asset may be published. Source/tooling-only distribution requires the
[distribution audit](distribution-audit.md) and independent license/provenance
review. Native game completion cannot override this policy.

Separately, a **local engineering milestone** requires static CPU execution,
live game-driven native GPU rendering, platform services, audio, controls,
saves and restart without JIT, an emulated Maxwell GPU or the general emulator
runtime. Synthetic tests and captured backgrounds alone do not qualify.
The milestone has not been completed and is not authorization to release a
playable compiled game.

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
archives only; running it does not authorize public upload without the source-only audit and manual review.
The repository workflows run emitter tests and distribution audits and do not publish releases.

Game-linked diagnostics require matching locally supplied title inputs. Keep title
code, generated game archives, assets, shaders, captures, keys and firmware out of
public commits and download assets. See [LEGAL.md](../LEGAL.md).

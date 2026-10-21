# Public distribution policy and audit

**Public game-linked binaries are prohibited.** This remains true even after the
complete native game works. Do not publish mechanically recompiled title code
or linked executables, shaders, game data, extracted assets, keys or firmware.
Reviewed authored source, documentation and synthetic tests are the only
current potential public deliverables.

## Current vs. retired path

The active native integration is [Belmante runtime](../integrations/belmante-runtime/README.md).
It generates title-specific modules from locally supplied extracted ExeFS and
RomFS data into private ignored directories. Those modules and source inputs
must not be committed, packaged or uploaded.

The older suyu-hosted baseline/keyed-export/re-export and installer scripts
were retired in October 2026. Its research findings are preserved as historical
references, not supported runnable workflows.

## Run the audit before a source-only publication

    python3 -m unittest discover -s tests -p 'test_dist_audit.py' -v
    python3 scripts/dist_audit.py
    python3 scripts/dist_audit.py --artifact dist/source-bundle.zip

The script checks Git-tracked current files for prohibited paths, binary/game
signatures and key-shaped assignments. If a dist/ directory exists, it is checked
automatically. Candidate directories and ZIP/TAR archives are inspected
member-by-member, with fail-closed limits on type, size, links and format.
Binaries, non-source payloads and private content paths are blocked.

CI runs this check for pushes, PRs and manual dispatch. The release-event hook
flags attached download assets, but runs **after** GitHub publication. Therefore
**never upload releases manually**. A future publication workflow must inspect
the exact final archive **before** upload and protect publishing credentials;
require the CI check in branch rules where possible.

## Limitations

This is a conservative policy guard, not a legal opinion, provenance proof
or substitute for manual source review. It does not inspect old Git objects,
submodule history, remote Actions artifacts, ignored local directories, external
dependencies, misleading names, or content hidden inside encoded source.
Removing files from main does not remove them from historical commits.

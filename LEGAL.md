# Legal position

This is the project's own statement of what it does, what it refuses to do, and
why it believes that is lawful. It is not legal advice and it is not a defence
brief. It exists so that anyone evaluating the repository — a user, a
contributor, a rights holder — can see the rules the project holds itself to
without reading the source.

## What this repository distributes

Our own code, build tooling and documentation, plus a submodule pointer to a
fork of an emulator. The tracked tree includes scripts, docs, tests and reviewed
native-runtime source patches. Native-foundations prereleases may also contain
verification tools and synthetic fixtures built from that public source. Those
tools contain no mechanically translated game code or game assets.

## What it does not distribute, under any circumstances

- **Game data.** No ROM, no dump, no extracted ExeFS or RomFS, no NSO, NCA, NSP
  or XCI, in whole or in part.
- **Keys.** No `prod.keys`, no `title.keys`, no key seeds, no derived key
  material, and no instructions for obtaining them or links to where they can
  be found.
- **Firmware.** None, in any form.
- **Recompiled game code.** The recompiler's output is a mechanical translation
  of copyrighted machine code and is therefore a derivative work of the game.
  It is generated locally, gitignored, and never published — not as source, not
  as an object file, not inside a binary. This is the project's firmest rule,
  because it is the one where the law is least ambiguous.

`generated/`, `traces/`, `targets/`, `keys/` and `firmware/` are all ignored,
and the ignore rules exist specifically to make the above difficult to violate
by accident. Coverage manifests under `targets/` quote instruction encodings
out of the title being surveyed, which is why that directory is ignored
alongside the others rather than treated as documentation.

## What you have to supply yourself

A legally dumped copy of software you own, and keys from hardware you own. The
project will not help you obtain either, and requests to do so will not be
answered.

## Licensing

GPL-3.0-or-later, matching upstream. The emulator fork carries
[`PROVENANCE.md`](https://github.com/dougchansan/suyu-v0.0.4/blob/mk8-recomp/PROVENANCE.md),
which records the base commit it descends from and enumerates the significant
changes, as GPL-3 §5(a) requires. Complete corresponding source for everything
distributed here is public in these repositories.

## On emulation

Reimplementing a console's behaviour is lawful. *Sony Computer Entertainment v.
Connectix* (9th Cir. 2000) and *Sony Computer Entertainment v. Bleem* (9th Cir.
2000) hold that developing an emulator — including the intermediate copying
that reverse engineering requires — is fair use. This project is a static
recompilation research effort built on that footing: its subject is translating
one instruction set into another, and its output of interest is a measurement,
not a playable port.

The current engineering goal includes a standalone native runtime for locally
supplied title code and data. Public distributions remain limited to the tools,
authored runtime code, synthetic fixtures and corresponding source described
above. This repository takes no position on how anyone else uses those tools,
and it is not a vehicle for playing games you do not own.

## Scope, honestly stated

The full native runtime is unfinished. The earlier suyu-hosted pipeline has
verified strict-static CPU race runs without JIT fallback. The independent
native CPU, service and Vulkan components are being integrated; their synthetic
tests do not establish standalone native gameplay. Progress and current build
limits are recorded in `docs/native-runtime-progress.md` and
`docs/native-builds.md`.

## Contact

If you are a rights holder with a concern about this repository, open an issue
or contact the repository owner through GitHub. Specific, actionable complaints
will get a specific, prompt response.

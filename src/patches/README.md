# Remaining patches

Everything that used to live here is now commit history on the suyu fork —
`third_party/suyu`, branch `mk8-recomp` — and the authoritative change set is:

```
git -C third_party/suyu diff d1d09321d7ab84252291e05b3efbc8a8dfa57481..mk8-recomp
```

One patch survives, because it does not apply to that repository.

## 0002-mcl-boost-variant-relative-include.patch

`externals/mcl/include/boost/variant.hpp` inside the **dynarmic** submodule has
a hardcoded home directory in an include path, which fails to build anywhere
except the machine it was written on.

That file is two submodules deep — `suyu/externals/dynarmic/externals/mcl` — so
the suyu fork cannot carry the fix. Retiring this patch needs a fork of
`suyu-emu/dynarmic` (which is *not* archived) as well, and then repointing
suyu's submodule at it.

Applied by `scripts/bootstrap.ps1`.

# Linux hardware control qualification

The 2026-10-23 UTC follow-up passed bounded authored controls on an RTX 3090
after reloading the same installed NVIDIA 580.159.03 driver. No new Xid fault
or Vulkan validation error occurred during the run or the closure review.
This establishes a usable test condition for these controls. Permanent repair,
the original fault's root cause and full native Linux gameplay remain unproven.

Earlier runs intermittently lost the device during `vkCreateDevice`, including
an independent native Linux test with no application GPU submissions. Clearing
an unrelated compute context did not prevent recurrence. Supported GPU resets
were refused because the device was primary. Those failed observations remain
part of the evidence; the subsequent pass does not erase them.

With explicit operator approval, GPU monitoring was stopped briefly and all
four NVIDIA modules were normally unloaded and reloaded. The installed and
loaded driver version, GPU identity, firmware version and captured effective
module parameters matched afterward. Both monitoring services and the same
user polling command, working directory, user and output were restored. The
host daemon stayed active and the paused work queue stayed paused. No force
unload, installation or reboot was used.

The fresh sequential qualification stopped on the first unexpected failure or
new Xid. All planned stages completed:

| Control | Verified coverage |
| --- | --- |
| Native Linux device creation | 100 devices: 50 minimal and 50 captured runtime feature configurations; no application submissions |
| Native Linux transfer | 3 devices, 48 submissions, 49,152 checked words |
| R353 full GPU fixture under Wine | 8 runs, 3,253 checks each |
| R353 expected command-arena failures under Wine | 24 runs reached the intended failure assertion and expected exit |
| R353 GPU timestamp phases under Wine | 8 runs, 412 checks and checksum 225485787456 each |
| R353 synchronization, cutoff and readback under Wine | 300, 300 and 259 checks respectively |

The 43 Wine controls alternated default and explicit NVIDIA driver enumeration
where applicable. Each selected the actual RTX 3090 with validation enabled.
The Windows ABI executable and native CRT group were pinned to the existing
verified R353 fixture. No game code or assets were loaded. Native Linux
creation/transfer and Windows ABI fixtures under Wine have separate scopes;
neither establishes a complete native Linux game port or a gameplay FPS result.

An initial harness admission error occurred before any GPU test: the user
process could not write-open the root-owned maintenance lock. Read-opening the
same lock with exclusive `flock` corrected that admission path. The original
failure was retained, and no GPU assertion was changed or skipped.

All owned tests closed, monitoring remained restored and the private archive's
1,560 manifest files passed local CRC, size and SHA-256 verification. Detailed
logs, hardware identifiers and executables remain private. Future controls
must stop on a new Xid or validation failure. These results do not change the
[CPU/GPU cost measurements](native-runtime-cost-profile.md) or demonstrate an
8 ms or 4 ms presentation interval.

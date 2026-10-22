# Render state before an independent frame loop

The normal-speed goal is 125 FPS, then 250 FPS, with simulation kept at its
reference cadence. Neither the sustained 8 ms nor the 4 ms presentation target
is verified. A draw-free callback or a stable race timer does not establish a
safe simulation/render boundary.

Three closed R344 investigations add 58 readable before/after state observations
and 29 observations of an actual model-preparation receiver. Each sampled window
has no dropped callback/state records, fatal errors or shader misses. They use
Stadium Time Trials with Mario and standard equipment at normal 60 Hz; engine
class was not verified. Steering and an off-track/wrong-way case provide different
state coverage. These are sparse observations and separate scenarios, not a
matched performance comparison or a lifetime/incarnation guarantee.

The wider payload contains five affine-shaped transforms. The earlier 144-byte
capture covered the first three. The two later blocks match in all 29 steering
samples, but a conditional producer path can modify the fifth independently.
Their maximum squared basis-length deviation is approximately 0.000297, with
maximum cross-column dot product 0.00000544. Treating every basis as an exact pure
rotation would discard measured terms. These samples do not establish a general
scale/shear, teleport or interpolation policy.

The model consumer reads beyond the old three-transform span. Its later
preparation callback also reaches animation accumulation and pose-adjustment
code. A private exact-callee experiment with authored getter/setter fixtures
passes 3,360 checks over 96 cases. Repeating a non-unit correction compounds the
current vector in 64 cases; unity is stable in 32. Restoring the input before
correction reproduces the first result in all 96 cases. Invalid model indices
leave the vector unchanged.

This proves a property of the isolated correction with those authored services.
It does not establish the linked title's virtual implementations or whether the
complete callback resets upstream pose. A safe extra render evaluation needs
known pose ownership and appropriate reset/evaluation order, alongside separate
animation advancement. All camera, object, particle and world consumers still
need coverage. Whole-task update gates and blind repetition of model-update
bodies remain unsuitable foundations for a high-refresh claim.

The two snapshot windows average 16.6660 and 16.6656 ms per presentation. The
later hook window averages 16.6928 ms and retains its 46.1823 ms maximum. GPU and
CPU-only times are not presentation budgets. Private code, selectors, captured
bytes, shaders and images are excluded from the public recipe.

R347 adds a bounded, opt-in GPU uniform capture window to connect CPU observations
to actual shader inputs. See [capture configuration](native-timing-measurements.md#bounded-gpu-uniform-capture).
It changes no scheduling, clocks, original draws, queries or synchronization.

A subsequent closed joint capture verifies 41 consecutive 256-byte CPU states
with all 40 adjacent final/entry boundaries matching, alongside 64 actual GPU
uniform snapshots. All 64 attempts fill the diagnostic budget in the first
included observed frame and one submission; later requested frames therefore
have no demonstrated GPU coverage. A zero present identifier is unassigned.
The observed CPU receiver differs from earlier runs, and no exact contiguous
48-byte matrix match was found in the captured GPU payloads. Derived or transposed
transforms, unrelated shader coverage and receiver identity remain unresolved.
Neither readable memory nor a matching address establishes object incarnation.
These results verify the diagnostic, not a safe independent rendering boundary.

# Verification — 0.3.1

## Game evidence and diagnosed defects

The user's 0.2.0 log is preserved locally in `diagnostics/0.2.0-user-test.log`, excluded from archives. It contains 35 contact timeouts. Nineteen had state 0 (on ground), surfaceInfo state 0 (unsupported), and CHARACTER_FLAGS::kSupport set. Thirteen were grounded with no support flag; the remainder were other cases. The 0.2.0 predicate incorrectly required surfaceInfo support even when the native ground state/support flag agreed. Many of these exits happened before the slide animation reached recovery.

Source inspection also confirms crouch cancellation/contact timeout directly sent a synthetic slide-stop event. This bypassed the remaining animation, explaining the missing recovery for those exits.

## Revision

0.3.1 adds single-snapshot normal validation at all three normalization call sites, a finite-output guard before writing controller velocity, and deferred graph-dependent cleanup. Pending cleanup survives Reset and is retried even after the local slide is inactive; new slide requests are rejected until delivery. Retry dispatch is guarded against reentrant callbacks. No forced unlocking of unrelated controls is introduced.

Native contact accepts on-ground state plus valid normal and either the support flag or supported/sliding surfaceInfo. If native evidence is inconclusive, a main-update ray probes from 12 game units above the feet to 24 below. The player's collision group is retained; static, terrain, ground and stair-helper layers with upward normals are accepted. The ray runs under the Havok world's read lock. Its copied result is published under a separate mutex after the world lock is released. Physics callbacks read only that snapshot, never cast rays, and reject snapshots older than 100 ms. Probe evidence does not override non-grounded, no-sim or ragdoll states. There is no ground snapping or fabricated support flag.

Cancellation, contact timeout and stall now latch recovery, advance the animation normally, and allow its actual recovery and stop annotations to fire. Continued downhill sensing cannot hold the clip again once recovery is requested. A one-second no-progress watchdog handles broken animation exits. Unsafe actor/graph/controller states retain hard cleanup. Native flat-ground timing remains unchanged.

## Checks

Additional regression cases reject zero, short, downward, infinite and NaN normals; retain cleanup through 100 unavailable-graph attempts; deliver once after graph restoration even under reentrant callbacks; and handle a subsequent interrupted slide.

MSVC 19.44 x64 Release build and CTest. Regression cases cover the exact support/surfaceInfo disagreement from the log; supported/sliding surfaces; fresh versus absent probe evidence; airborne, ragdoll and invalid-normal rejection; normal recovery despite a continued steep downhill at 30/60/144Hz; stock flat duration across three playback speeds; uphill/downhill timing; 120-second sustained slopes; transition boundaries; contact grace; speed cap and cancellation input.

`verify_build.py` checks x64 architecture, SKSE exports, version 0.3.1, Skyrim 1.6.1170 metadata, exact archive/staged DLL and INI equality, no SkyParkour assets in the install ZIP, and unchanged Ground SE/jump DLL hashes. The supplied first/third-person clips' recovery at 0.783333 and stop at 0.850000 animation seconds remain after the retimed window ending at 0.70.

## Limits

The user's 0.3.0 playtest logged 42 starts and 42 native exits, with no warnings, errors or recovery timeouts, and the user reported improved behavior. No 0.3.1 in-game test has been performed. Pure tests validate contact decisions, cleanup retry state and animation timing; they do not execute Skyrim or validate Havok raycast results or event delivery through a recreated animation graph. Both reference source checkouts and user logs are excluded from distributable archives.

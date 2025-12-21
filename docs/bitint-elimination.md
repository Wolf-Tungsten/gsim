# BitInt elimination (current state)

## What changed
- Removed `_BitInt` codegen entirely; widths >64 now map to stack-backed `GsimWideU/S` (`include/common.h`).
- Replaced the GMP-dependent wrapper with `include/GsimInt.h`, a fixed-limb, allocation-free bigint used by generated simulators.
- Shadow/copilot paths and dual-backend builds are gone to keep runtime lean.

## Runtime model
- <=64-bit signals: native integers.
- >64-bit signals: `GsimWideU/S` (stack only, width-masked, two’s complement when signed).
- Difftest helpers and generated models include only the `Gsim*` types; GMP is needed at build time for the generator, not at runtime.

## Validation
- Existing flows (`make run dutName=ysyx3`) pass and print “Hello, RISC-V World!”; this simulator is the correctness reference.

## Remaining risks
- Extremely wide vectors may still be costly due to limb math; profile before further tuning.
- Keep build flags tuned (`-O3 -march=native`; LTO optional) and profile hot paths (perf/gprof) if regressions appear.

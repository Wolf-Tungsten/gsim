# GMP migration plan (replace `_BitInt`)

## Goals
- Drop reliance on `_BitInt` in generated simulators to relax compiler requirements and widen toolchain support.
- Keep the fast path for narrow signals (<=64 bits) while routing all wider arithmetic to GMP.
- Preserve runtime performance by limiting allocations/copies and keeping masking overhead low when using GMP-backed paths.

## Current `_BitInt` footprint
- Type selection macros emit `_BitInt` for widths above 64 bits (`include/common.h:50-66`).
- Generated C++ headers enforce `__BITINT_MAXWIDTH__` and a clang-19 floor (`src/cppEmitter.cpp:182-191`).
- Difftest signal checker code-gen builds `_BitInt` temporaries to reconstruct wide values (`scripts/sigFilter.py:40-92`).

## Proposed GMP runtime model
- Use native integers for <=64-bit signals; introduce a GMP-backed wrapper (e.g., `GmpInt`) for wider values that stores `mpz_class` plus declared width.
- Provide width-aware helpers (masking after every write/op, bit-slice, concat, shifts, comparisons) so generated code can stay readable and constant folding remains correct.
- Expose conversions between `GmpInt` and raw `uint64_t` chunks to interop with Verilator `WData` arrays and existing harness I/O.
- Treat performance as first-class: reuse scratch buffers, avoid constructing/destroying `mpz_class` inside hot loops, and keep helper APIs allocation-conscious.

## Replacement plan (two-phase rollout)
1. **Phase 1: GMP shadow path (fine-grained, single binary)**
   - Add a GMP helper header (e.g., `include/gmp_int.h`) defining `GmpInt`, constructors from `uint64_t`/arrays, masking, and common ops (add/sub/mul/div/mod/and/or/xor/not/neg/concat/bits/shl/shr/mux/compare), with minimal heap traffic.
   - Keep existing `_BitInt`-based types/macros; add parallel `widthUTypeGmp/widthSTypeGmp` (or equivalent) and emit both representations into a single generated binary: GMP is the authoritative path, `_BitInt` is a shadow/copilot for correctness checks.
   - In codegen (`src/instsGenerator.cpp`, `src/cppEmitter.cpp`), emit dual values per signal (or dual compute paths) so runtime can evaluate with GMP and `_BitInt` side by side; ensure signatures and diff scripts align to the dual layout.
   - Rewrite `scripts/sigFilter.py` (or add a dual mode) to emit comparisons that, within one executable, check per-signal equality between GMP and `_BitInt`; fail fast on first mismatch, optionally dumping the signal name and values.
   - Run existing Makefile flows once; the produced simulator performs self-checks (GMP result vs `_BitInt` shadow) during execution. Track performance of the GMP primary path and keep shadow checks guarded/optional to bound overhead (e.g., compile-time flag or runtime knob).
2. **Phase 2: Remove `_BitInt`**
   - Switch `widthUType/widthSType` to the GMP-backed wrappers for widths >64; drop `_BitInt` capability checks and legacy codepaths.
   - Update `bitMask`/`legalCppCons` and emitted headers to rely solely on GMP helpers; remove `_BitInt`-specific code/comments.
   - Clean diff scripts and harness to use GMP paths only; keep native <=64-bit fast paths.
   - Keep Phase 1 shadow support as an opt-in compile-time flag (e.g., `-DENABLE_GMP_SHADOW=1`) so production builds stay on the GMP-only path without overhead.
   - Regenerate sample outputs, run Makefile flows, and verify performance remains within prior targets.

## Validation
- Reuse existing Makefile flows (`make build-gsim`, `make run dutName=<preset>`, ready-to-run designs) under GCC/clang.
- Phase 1: a single generated simulator carries both GMP (primary) and `_BitInt` (shadow) paths; enable per-signal self-checks and fail fast on mismatches using existing difftest/compare targets (including wide >256-bit signals). Keep a flag to disable shadow checks for performance measurements.
- Phase 2: after dropping `_BitInt`, rerun the same flows and perf targets/counters to confirm correctness and runtime stability; no new cases needed.

## Risks / open points
- GMP performance and heap churn for very wide vectors; may need pooling or scratch buffers.
- Copy semantics of `mpz_class` vs `mpz_t` in hot paths; wrapper design should avoid extra allocations.
- Interaction with Verilator scheduling if `GmpInt` constructors/destructors become expensive; consider arena allocation for generated state arrays.

# GMP migration plan (replace `_BitInt`)

## Goals
- Remove `_BitInt` reliance so generated simulators build on broader toolchains without raising compiler version requirements.
- Keep the native fast path for widths <=64 bits and route wider arithmetic through a GMP-backed wrapper.
- Protect runtime performance by minimizing allocations/copies inside hot loops and keeping masking overhead low.

## Status
- Type-selection macros now map widths above 64 bits to GMP-backed types only (`include/common.h`); `_BitInt` is no longer emitted.
- The previous shadow/copilot path that paired GMP with `_BitInt` inside one binary has been removed from codegen and build scripts to avoid extra runtime overhead.

## Runtime model
- <=64-bit signals use standard integers; wider signals use `GmpWideU/S` wrappers over `mpz_class` with width-aware masking.
- Wrappers support arithmetic, bitwise ops, shifts, comparisons, and chunked import/export for Verilator-style arrays while normalizing results to the declared width.
- Performance remains the priority: we reuse `mpz_class` storage, avoid unnecessary temporaries in generated code, and keep masking logic centralized inside the helper.

## Rollout
1. **Phase 1 (complete, retired)**: Built a single simulator carrying both GMP (primary) and `_BitInt` (shadow) paths to validate equivalence. This path is now removed from production builds for speed.
2. **Phase 2 (current)**: Production code is GMP-only. `widthUType/widthSType` emit GMP types for widths >64; code generators and difftest helpers use GMP exclusively, with no shadow or `_BitInt` fallback compiled in.

## Validation
- Reuse existing Makefile flows (`make build-gsim`, `make run dutName=ysyx3`, and ready-to-run presets). No new cases are required.
- The GMP-only simulator is the correctness reference; regression runs should confirm identical behavior with reduced runtime overhead.

## Risks / follow-ups
- GMP allocation churn for extremely wide vectors; profile hot paths and consider pooling or tuned GMP builds if performance regresses.
- Ensure linked GMP libraries are optimized on target platforms to preserve runtime performance gains.

## Speedtest
```
cycles 10010000 (99380 ms, 100724 per sec) simulation process 91.00% 
[    1.350000] Freeing unused kernel memory: 60K
[    1.370000] This architecture does not havcycles 10120000 (100482 ms, 100714 per sec) simulation process 92.00% 
e kernel memory protection.
cycles 10230000 (101593 ms, 100695 per sec) simulation process 93.00% 
cycles 10340000 (102705 ms, 100676 per sec) simulation process 94.00% 
cycles 10450000 (103938 ms, 100540 per sec) simulation process 95.00% 
Hello, RISC-V World!
hanging
cycles 10560000 (105003 ms, 100568 per sec) simulation process 96.00% 
cycles 10670000 (106040 ms, 100622 per sec) simulation process 97.00% 
cycles 10780000 (107122 ms, 100632 per sec) simulation process 98.00% 
cycles 10890000 (108138 ms, 100704 per sec) simulation process 99.00% 
cycles 11000000 (109172 ms, 100758 per sec) simulation process 100.00% 
109.15user 0.05system 1:49.23elapsed 99%CPU (0avgtext+0avgdata 86128maxresident)k
0inputs+0outputs (0major+20824minor)pagefaults 0swaps
make[2]: Leaving directory '/home/gaoruihao/gsim'
make[1]: Leaving directory '/home/gaoruihao/gsim'
114.67user 0.58system 1:55.21elapsed 100%CPU (0avgtext+0avgdata 198460maxresident)k
0inputs+5064outputs (0major+133063minor)pagefaults 0swaps
```
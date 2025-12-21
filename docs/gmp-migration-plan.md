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

## Performance optimization plan
- [done] Cut allocations: move the wrapper to reuse `mpz_t` storage (preallocate with `mpz_init2`) and avoid constructing transient `mpz_class` objects in hot paths; codegen already reuses temporaries for concat/shift/compare.
- [done] Trim masking: centralize normalization at mutation points (assignments and in-place ops) to keep width-bounded results.
- [done] Stay low-level: replace `mpz_class` helpers in the wrapper with direct `__gmpz_*` calls for arithmetic/bitwise ops; narrow (<=64-bit) paths stay on native integers.
- [todo] Build for the host: compile the simulator with `-O3 -march=native` (and LTO if acceptable); ensure GMP is built with platform-tuned assembly (e.g., `--enable-fat` or a native-tuned build) to reduce limb overhead.
- [todo] Measure before/after: profile with `perf` or sampling to confirm hotspots (allocation/normalization vs arithmetic), then iterate on the highest-impact sites.

### Latest profile (user-mode gprof, -pg, ysyx3)
- Run: `BUILD_DIR=build-prof GMON_OUT_PREFIX=build-prof/gmon.out CXXFLAGS='-pg -O3' LDFLAGS='-pg' make run dutName=ysyx3`
- Top self time: `Snewtop::subStep0` ~77% (main simulator loop).
- GMP hot spots (self time): `GmpInt<128,false>::operator&` ~2.6%, `GmpInt<128,false>::GmpInt<int>` ~2.4%, unary minus ~2.1%, bitwise OR ~1.5%, `operator unsigned long` ~1.4%; canonicalization no longer dominates.
- Remaining work: focus on reducing temporary constructions and add fixed-limb bitwise/shift fast paths for 128/192-bit widths to trim the bitwise/constructor costs.

## Speedtest
```
[    1.350000] Freeing unused kernel memory: 60K
[    1.370000] This architecture does not havcycles 10120000 (53690 ms, 188489 per sec) simulation process 92.00% 
e kernel memory protection.
cycles 10230000 (54281 ms, 188463 per sec) simulation process 93.00% 
cycles 10340000 (54869 ms, 188448 per sec) simulation process 94.00% 
cycles 10450000 (55467 ms, 188400 per sec) simulation process 95.00% 
Hello, RISC-V World!
hanging
cycles 10560000 (56025 ms, 188487 per sec) simulation process 96.00% 
cycles 10670000 (56569 ms, 188619 per sec) simulation process 97.00% 
cycles 10780000 (57129 ms, 188695 per sec) simulation process 98.00% 
cycles 10890000 (57660 ms, 188865 per sec) simulation process 99.00% 
cycles 11000000 (58202 ms, 188996 per sec) simulation process 100.00% 
58.19user 0.05system 0:58.26elapsed 99%CPU (0avgtext+0avgdata 86332maxresident)k
0inputs+0outputs (0major+20829minor)pagefaults 0swaps
make[2]: Leaving directory '/home/gaoruihao/gsim'
make[1]: Leaving directory '/home/gaoruihao/gsim'
58.34user 0.36system 0:58.83elapsed 99%CPU (0avgtext+0avgdata 86332maxresident)k
0inputs+0outputs (0major+46535minor)pagefaults 0swaps
```

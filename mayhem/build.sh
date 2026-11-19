#!/usr/bin/env bash
# mayhem/build.sh — build chatterino2's fuzz targets (sanitized) and its own gtest suite (normal flags).
#
# Both builds use upstream's CMake as-is. The fuzz executables are added without editing upstream
# files: -DCMAKE_PROJECT_INCLUDE=mayhem/fuzz-hook.cmake defers mayhem/fuzz-targets.cmake to the end of
# the top-level CMakeLists.txt, where it links each harness against chatterino-lib (+ mocks) exactly
# as tests/CMakeLists.txt links chatterino-test. Re-runnable (incremental) and offline: every
# dependency is an apt package or a checked-out submodule.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS
SRC="${SRC:-/mayhem}"
cd "$SRC"

common=(-G Ninja -DBUILD_APP=OFF -DBUILD_TESTS=ON -DUSE_PRECOMPILED_HEADERS=OFF
        -DCHATTERINO_UPDATER=OFF -DBUILD_WITH_LIBNOTIFY=OFF)

# 1) Sanitized + coverage-instrumented project and harnesses.
mkdir -p build-fuzz
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build-fuzz/standalone_main.o
# LeakSanitizer off for every fuzz + standalone binary (leaks are not the bug class fuzzed here).
"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build-fuzz/lsan_off.o
fuzzflags="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
cmake -S . -B build-fuzz "${common[@]}" -DCHATTERINO_PLUGINS=OFF \
    -DCMAKE_PROJECT_INCLUDE="$SRC/mayhem/fuzz-hook.cmake" \
    -DMAYHEM_STANDALONE_OBJ="$SRC/build-fuzz/standalone_main.o" \
    -DMAYHEM_LSAN_OFF_OBJ="$SRC/build-fuzz/lsan_off.o" \
    -DCMAKE_C_FLAGS="$fuzzflags" -DCMAKE_CXX_FLAGS="$fuzzflags"
targets=(fuzz_irc_message fuzz_eventsub_message fuzz_link_parser fuzz_filter
         fuzz_seventv_eventapi fuzz_pubsub_message fuzz_bttv_liveupdate fuzz_recent_messages)
build_targets=()
for t in "${targets[@]}"; do build_targets+=("$t" "$t-standalone"); done
cmake --build build-fuzz -j"$MAYHEM_JOBS" --target "${build_targets[@]}"
for t in "${targets[@]}"; do
    install -m 0755 "build-fuzz/fuzz/$t" "$SRC/$t"
    install -m 0755 "build-fuzz/fuzz/$t-standalone" "$SRC/$t-standalone"
done

# 2) Upstream's test suite with normal flags (mayhem/test.sh only runs it).
cmake -S . -B build-tests "${common[@]}" -DCHATTERINO_PLUGINS=ON \
    -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build-tests -j"$MAYHEM_JOBS" --target chatterino-test

#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Upstream's gtest suite (built by build.sh with normal flags). Run the offline, deterministic
# suites: IRC message parsing snapshots (golden JSON of every built message), link parsing, the
# filter language, IRC/tag helpers, formatting helpers, emojis. Counts come from gtest's XML report,
# so a neutered binary (no report) fails.
bin="$SRC/build-tests/bin/chatterino-test"
[ -x "$bin" ] || { echo "missing $bin — build.sh did not build the test suite" >&2; emit_ctrf gtest 0 1; exit 1; }
filter='*IrcMessageHandler*:LinkParser*:Filters*:IrcHelpers*:Helpers*:FormatTime*:Emojis*:UtilTwitch*'
xml="$(mktemp -d)/gtest.xml"
export QT_QPA_PLATFORM=minimal
( cd "$(mktemp -d)" && "$bin" --gtest_filter="$filter" --gtest_output="xml:$xml" ) > /tmp/chatterino-test.log 2>&1
rc=$?
tail -5 /tmp/chatterino-test.log
if [ ! -s "$xml" ]; then
  echo "no gtest XML report (exit $rc)" >&2; emit_ctrf gtest 0 1; exit 1
fi
read -r tests failures disabled skipped < <(python3 - "$xml" <<'PY'
import sys, xml.etree.ElementTree as ET
r = ET.parse(sys.argv[1]).getroot()
print(r.get("tests"), int(r.get("failures", 0)) + int(r.get("errors", 0)), r.get("disabled", 0), r.get("skipped", 0) if r.get("skipped") else 0)
PY
)
passed=$(( tests - failures - skipped - disabled ))
[ "$rc" -eq 0 ] || [ "$failures" -gt 0 ] || failures=1
[ "$tests" -gt 0 ] || failures=1
emit_ctrf gtest "$passed" "$failures" "$(( skipped + disabled ))"

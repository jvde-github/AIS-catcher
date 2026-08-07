#!/bin/bash
#
# Parser throughput benchmark / regression test.
#
# Exercises the NMEA + JSON hot paths (SWAR scanning in Source/Library/SWAR.h and
# its callers) by replaying a plain-text NMEA file through the decoder.
#
#   ./scripts/bench-parser.sh                       # bench ./build/AIS-catcher
#   ./scripts/bench-parser.sh path/to/AIS-catcher   # bench a specific binary
#   ./scripts/bench-parser.sh a/AIS-catcher b/AIS-catcher   # A/B two builds
#
# Env:
#   NMEA=<file>    source NMEA text (default: build/nmea, else nmea_1M.txt)
#   SIZE_MB=<n>    input is looped up to ~n MB (default 300)
#   RUNS=<n>       timed runs per workload, best is reported (default 5)
#   KEEP=1         keep the generated input file for the next invocation
#
# Two input types are exercised, each at two output levels:
#
#   NMEA text input   -q     parse + decode        -> NMEA.cpp scanLine, Message.cpp
#                     -o 5   + JSON output         -> adds JSON/Writer.h escaping
#   JSON input        -q     parse + decode        -> NMEA.cpp scanJSON, JSON/Parser.cpp
#                     -o 5   + JSON output         -> full JSON in -> JSON out round trip
#
# The JSON input is produced by running -o 5 over the NMEA input and feeding the
# result back in; AIS-catcher auto-detects a leading '{' and switches to the JSON
# reader, so both use -r txt.
#
# Output of every binary is compared (modulo receive timestamps); any mismatch
# is a hard failure, so this doubles as a correctness check for hot-path edits.

set -u

cd "$(dirname "$0")/.."
ROOT=$(pwd)
WORK=${TMPDIR:-/tmp}/aiscatcher-bench
SIZE_MB=${SIZE_MB:-300}
RUNS=${RUNS:-5}

BINS=("$@")
[ ${#BINS[@]} -eq 0 ] && BINS=("$ROOT/build/AIS-catcher")

for b in "${BINS[@]}"; do
	[ -x "$b" ] || { echo "not executable: $b" >&2; exit 1; }
done

# ---- input ----------------------------------------------------------------
if [ -z "${NMEA:-}" ]; then
	for c in "$ROOT/build/nmea" "$ROOT/nmea_1M.txt"; do
		[ -f "$c" ] && { NMEA=$c; break; }
	done
fi
[ -n "${NMEA:-}" ] && [ -f "$NMEA" ] || {
	echo "no NMEA input found; set NMEA=<file>" >&2; exit 1; }

mkdir -p "$WORK"
BIG=$WORK/nmea_${SIZE_MB}mb.txt
if [ ! -f "$BIG" ]; then
	src_mb=$(( $(stat -c %s "$NMEA") / 1048576 ))
	[ "$src_mb" -lt 1 ] && src_mb=1
	loops=$(( (SIZE_MB + src_mb - 1) / src_mb ))
	echo "building $BIG (${loops}x $(basename "$NMEA"), ~${SIZE_MB} MB)..."
	: > "$BIG"
	for _ in $(seq 1 "$loops"); do cat "$NMEA"; done >> "$BIG"
fi
BYTES=$(stat -c %s "$BIG")
LINES=$(wc -l < "$BIG")
echo "nmea input: $BIG  ($((BYTES / 1048576)) MB, $LINES sentences)"

# JSON input: -o 5 output of the same data, fed back in. AIS-catcher switches to
# the JSON reader on a leading '{', so this exercises scanJSON + JSON::Parser.
# -o 5 inflates ~10x, so seed from the un-looped source and size to ~SIZE_MB.
JBIG=$WORK/json_${SIZE_MB}mb.txt
if [ ! -f "$JBIG" ]; then
	echo "building $JBIG (-o 5 over $(basename "$NMEA"), sized to ~${SIZE_MB} MB)..."
	seed=$WORK/json_seed.txt
	[ -f "$seed" ] || "${BINS[0]}" -r txt "$NMEA" -o 5 2>/dev/null > "$seed"
	seed_mb=$(( $(stat -c %s "$seed") / 1048576 ))
	[ "$seed_mb" -lt 1 ] && seed_mb=1
	if [ "$seed_mb" -ge "$SIZE_MB" ]; then
		head -n $(( $(wc -l < "$seed") * SIZE_MB / seed_mb )) "$seed" > "$JBIG"
	else
		: > "$JBIG"
		for _ in $(seq 1 $(( (SIZE_MB + seed_mb - 1) / seed_mb ))); do cat "$seed"; done >> "$JBIG"
	fi
	rm -f "$seed"
fi
JBYTES=$(stat -c %s "$JBIG")
JLINES=$(wc -l < "$JBIG")
echo "json input: $JBIG  ($((JBYTES / 1048576)) MB, $JLINES messages)"
echo

# ---- correctness: all binaries must agree, byte for byte -------------------
# rxtime/rxuxtime are wall-clock and differ between runs.
norm() { sed -E 's/"rxtime":"[0-9]*"//; s/"rxuxtime":[0-9]*//'; }

JSAMPLE=$WORK/json_sample.txt
head -n 100000 "$JBIG" > "$JSAMPLE"

for src in "$NMEA" "$JSAMPLE"; do
	echo "== output equivalence ($(basename "$src")) =="
	ref=""
	for b in "${BINS[@]}"; do
		h=$("$b" -r txt "$src" -o 5 2>/dev/null | norm | md5sum | cut -d' ' -f1)
		printf "  %-40s %s\n" "$(basename "$(dirname "$b")")/$(basename "$b")" "$h"
		if [ -z "$ref" ]; then ref=$h
		elif [ "$h" != "$ref" ]; then echo "  FAIL: output differs" >&2; exit 1; fi
	done
	echo "  ok"
	echo
done
rm -f "$JSAMPLE"

# ---- timing ---------------------------------------------------------------
best_of() {
	local bin=$1 input=$2; shift 2
	local best=""
	for _ in $(seq 1 "$RUNS"); do
		local t
		t=$( { /usr/bin/time -f "%e" "$bin" -r txt "$input" "$@" >/dev/null; } 2>&1 | tail -1 )
		best=$(awk -v a="$best" -v b="$t" 'BEGIN{print (a=="" || b+0<a+0) ? b : a}')
	done
	echo "$best"
}

bench() {
	local label=$1 input=$2 bytes=$3 lines=$4 workload=$5
	echo "== $label input, $workload  (best of $RUNS) =="
	local basetime=""
	for b in "${BINS[@]}"; do
		# shellcheck disable=SC2086
		local t; t=$(best_of "$b" "$input" $workload)
		local mbs mps rel
		mbs=$(awk -v x="$bytes" -v t="$t" 'BEGIN{printf "%.0f", x/1048576/t}')
		mps=$(awk -v n="$lines" -v t="$t" 'BEGIN{printf "%.2f", n/t/1e6}')
		[ -z "$basetime" ] && basetime=$t
		rel=$(awk -v a="$basetime" -v b="$t" 'BEGIN{printf "%+.1f%%", (b-a)/a*100}')
		printf "  %-40s %6ss  %5s MB/s  %5s M msg/s  %s\n" \
			"$(basename "$(dirname "$b")")/$(basename "$b")" "$t" "$mbs" "$mps" "$rel"
	done
	echo
}

for workload in "-q" "-o 5"; do
	bench "nmea" "$BIG"  "$BYTES"  "$LINES"  "$workload"
done
for workload in "-q" "-o 5"; do
	bench "json" "$JBIG" "$JBYTES" "$JLINES" "$workload"
done

[ "${KEEP:-0}" = "1" ] || rm -f "$BIG" "$JBIG"

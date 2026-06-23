#!/bin/bash
# Equivalence harness: prove a candidate core is bit-exact with the base core.
#
# Compares the final-state hash (printed by the tester regardless of the
# fixture-dependent expected-result checks) of the base core against a candidate
# core, in both Simple playback and the Rerecord save/load cycle. This is the
# authoritative base-vs-newN equivalence signal and is intentionally decoupled
# from demo/expected-result fidelity (the .lmp-derived demos may desync on the
# current core version without affecting equivalence).
#
# Usage: equiv.sh <baseTester> <candidateTester> <test> <sol> [rerecordDepth]

set -u
baseTester=${1}
candTester=${2}
test=${3}
sol=${4}
depth=${5:-1}

hashOf() { "$1" "$3" "$4" --cycleType "$2" --rerecordDepth "$depth" 2>/dev/null | grep 'Final State Hash' | awk '{print $NF}'; }

baseSimple=$(hashOf "$baseTester" Simple   "$test" "$sol")
candSimple=$(hashOf "$candTester" Simple   "$test" "$sol")
candRerec=$( hashOf "$candTester" Rerecord "$test" "$sol")

ok=1
[ -n "$baseSimple" ] || { echo "[equiv] FAIL: base produced no hash"; ok=0; }
if [ "$baseSimple" = "$candSimple" ]; then echo "[equiv] Simple   : MATCH   ($baseSimple)"; else echo "[equiv] Simple   : MISMATCH base=$baseSimple cand=$candSimple"; ok=0; fi
if [ "$baseSimple" = "$candRerec"  ]; then echo "[equiv] Rerecord : MATCH   ($candRerec)";  else echo "[equiv] Rerecord : MISMATCH base=$baseSimple cand=$candRerec"; ok=0; fi

[ "$ok" = 1 ] && { echo "[equiv] $test : PASS"; exit 0; } || { echo "[equiv] $test : FAIL"; exit 1; }

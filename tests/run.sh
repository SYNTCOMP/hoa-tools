#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin_dir=$(CDPATH= cd -- "${1:-$root}" && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/hoa-tools-tests.XXXXXX")
trap 'rm -rf "$scratch"' EXIT
trap 'exit 1' HUP INT TERM
ulimit -s 8192
ulimit -c 0
failures=0
tests=0

check() {
    expected=$1
    input=$2
    shift 2
    if "$@" < "$input" > "$scratch/stdout" 2> "$scratch/stderr"; then
        status=0
    else
        status=$?
    fi
    if [ "$status" -ne "$expected" ]; then
        printf 'Expected exit %s, got %s\n' "$expected" "$status" >&2
        return 1
    fi
    if grep -Eq 'AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:' "$scratch/stderr"; then
        return 1
    fi
}

report() {
    label=$1
    shift
    tests=$((tests + 1))
    if "$@"; then
        printf 'PASS: %s\n' "$label"
    else
        printf 'FAIL: %s\n' "$label" >&2
        head -n 20 "$scratch/stderr" >&2
        failures=$((failures + 1))
    fi
}

nonidentity() {
    check 0 examples/nonidentity-controllable.ehoa "$bin_dir/hoa2pg" &&
        cmp tests/expected/nonidentity-controllable.pg "$scratch/stdout"
}

reject() {
    tool=$1
    input=$2
    message=$3
    check 1 "$input" "$bin_dir/$tool" &&
        grep -Fq "$message" "$scratch/stderr"
}

nonparity() {
    check 100 "$2" "$bin_dir/$1" &&
        grep -Fq 'Expected "parity..." automaton' "$scratch/stderr"
}

priority_zero() {
    check 0 /dev/null python3 pg2hoa.py tests/inputs/priority-zero.pg &&
        grep -Fxq 'Acceptance: 1 Inf(0)' "$scratch/stdout" &&
        cp "$scratch/stdout" "$scratch/priority-zero.ehoa" &&
        check 0 "$scratch/priority-zero.ehoa" "$bin_dir/hoacheck"
}

report 'hoa2pg non-identity controllable AP order (exact game)' nonidentity
for tool in hoacheck hoa2pg hoa2aig; do
    report "$tool rejects out-of-range controllable AP" reject "$tool" \
        tests/inputs/out-of-range-controllable.ehoa \
        'Semantic error: controllable-AP index 2 is outside [0, 2)'
    report "$tool rejects duplicate controllable AP" reject "$tool" \
        tests/inputs/duplicate-controllable.ehoa \
        'Semantic error: duplicate controllable-AP index 1'
    report "$tool rejects examples/test2.ehoa" reject "$tool" \
        examples/test2.ehoa \
        'Semantic error: controllable-AP index 7 is outside [0, 7)'
    report "$tool full arbiter with 8 MiB stack" check 0 \
        examples/full_arbiter_unreal3.tlsf.ehoa "$bin_dir/$tool"
    for input in examples/nonidentity-controllable.ehoa examples/starve.ehoa \
        examples/lilydemo01.tlsf.ehoa tests/inputs/all-controllable.ehoa; do
        report "$tool accepts $input" check 0 "$input" "$bin_dir/$tool"
    done
    for input in examples/aut1.ehoa examples/test3.ehoa; do
        report "$tool rejects non-parity $input (exit 100)" nonparity "$tool" "$input"
    done
done
report 'pg2hoa priority 0 accepts Inf(0) and passes hoacheck' priority_zero

printf '%s tests, %s failures\n' "$tests" "$failures"
[ "$failures" -eq 0 ]

#!/usr/bin/env bash
# Run every algorithm and optimization variant on each row of the test dataset config.
#
# Each (variant, graph) pair is a separate cfg_bench -t process under a time limit and, when
# systemd-run --user works, a memory limit without swap. Prints one line per variant and the list
# of failed runs; the full output goes to results/test_dataset_<time>/log.txt. Exits with 1 when
# any run is not [OK].

set -uo pipefail
cd "$(dirname "$0")/.." || exit 2

config=tests/data/configs/all.csv
time_limit=3
memory=10G
only=""
dry_run=false

usage() {
    cat <<EOF
Usage: $0 [-c config] [-t seconds] [-m memory] [-a algorithm] [--dry-run]

  -c config     config to run (default: $config)
  -t seconds    time limit of one run on one graph (default: $time_limit)
  -m memory     memory limit of one run, no swap (default: $memory)
  -a algorithm  run only the variants of this algorithm, e.g. CFL_adv
  --dry-run     print the commands without running them
EOF
}

while (($#)); do
    case $1 in
    -c) config=$2; shift 2 ;;
    -t) time_limit=$2; shift 2 ;;
    -m) memory=$2; shift 2 ;;
    -a) only=$2; shift 2 ;;
    --dry-run) dry_run=true; shift ;;
    -h | --help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
    esac
done

binary=./build/cfg_bench
if ! $dry_run && [[ ! -x $binary ]]; then
    echo "$binary not found, run make first" >&2
    exit 2
fi
if [[ ! -f $config ]]; then
    echo "config not found: $config" >&2
    exit 2
fi

# the same variants as make CI
optimizations=("" -e -f -l -b -ef -el -eb -fl -fb -lb -efl -efb -elb -flb -eflb)
variants=()
for flags in "${optimizations[@]}"; do variants+=("CFL_adv $flags"); done
variants+=("CFL" "CFL_single_path" "CFL_all_path" "CFL_all_path --CFL-all-path-use-CFPQ-Core")
for flags in "${optimizations[@]}"; do variants+=("CFL_all_path_adv $flags"); done
variants+=(
    "CFL_CFPQ_RSM --compute-results"
    "CFL_CFPQ_RSM --use-start-nodes --compute-results"
    "CFL_multsrc --compute-results"
    "CFL_multsrc --use-start-nodes --compute-results"
)
for flags in "${optimizations[@]}"; do
    variants+=("CFL_multsrc_adv $flags --compute-results")
    variants+=("CFL_multsrc_adv $flags --use-start-nodes --compute-results")
done
variants+=("CFL_Kron")

limiter=()
if command -v systemd-run >/dev/null && systemd-run --user --scope -q true 2>/dev/null; then
    limiter=(systemd-run --user --scope -q -p "MemoryMax=$memory" -p MemorySwapMax=0)
else
    echo "warning: systemd-run --user is unavailable, runs have no memory limit" >&2
fi

mapfile -t rows < <(grep -v '^[[:space:]]*$' "$config")
row_config=$(mktemp)
trap 'rm -f "$row_config"' EXIT

log_dir=results/test_dataset_$(date +%Y-%m-%d_%H-%M-%S)
$dry_run || mkdir -p "$log_dir"
log=$log_dir/log.txt

failures=()
for variant in "${variants[@]}"; do
    read -ra args <<<"$variant"
    [[ -n $only && ${args[0]} != "$only" ]] && continue

    ok=0
    total=0
    slowest=0
    slowest_graph=-
    for row in "${rows[@]}"; do
        graph=$(basename "${row%%,*}" .g)
        IFS=, read -ra columns <<<"$row"
        if [[ " $variant " == *" --use-start-nodes "* && ${#columns[@]} -lt 4 ]]; then
            continue # the row has no start-node file
        fi

        printf '%s\n' "$row" >"$row_config"
        command=("${limiter[@]}" timeout "$time_limit" "$binary" -t -c "$row_config" -a "${args[@]}")
        if $dry_run; then
            echo "$graph: ${command[*]}"
            continue
        fi

        output=$("${command[@]}" 2>&1)
        code=$?
        printf '=== %s | %s | exit %d\n%s\n' "$variant" "$graph" "$code" "$output" >>"$log"
        output=$(sed 's/\x1b\[[0-9;]*m//g' <<<"$output")

        if ((code == 124)); then
            status="timeout (${time_limit}s)"
        elif ((code == 137)); then
            status="killed (memory limit $memory)"
        elif grep -q '\[OK\]' <<<"$output"; then
            status=OK
        elif grep -q '\[Wrong\]' <<<"$output"; then
            status="wrong: $(grep -o 'Result: [0-9]*.*Result must be [0-9]*' <<<"$output" | head -1)"
        elif grep -q '\[Failed\]' <<<"$output"; then
            status="failed: $(grep -m1 'LAGraph failure' <<<"$output" | cut -c1-120)"
        elif grep -q '\[Unknown\]' <<<"$output"; then
            status="unknown: the config has no expected result"
        else
            status="error, exit $code: $(tail -n1 <<<"$output" | cut -c1-120)"
        fi

        total=$((total + 1))
        if [[ $status == OK ]]; then
            ok=$((ok + 1))
            seconds=$(grep -o '([0-9.]* sec)' <<<"$output" | head -1 | tr -dc '0-9.')
            if [[ -n $seconds ]] && awk -v a="$seconds" -v b="$slowest" 'BEGIN { exit !(a > b) }'; then
                slowest=$seconds
                slowest_graph=$graph
            fi
        else
            failures+=("$variant | $graph | $status")
        fi
    done

    $dry_run || printf '%-50s %3d/%-3d OK   slowest %6.2fs  %s\n' "$variant" "$ok" "$total" "$slowest" "$slowest_graph"
done

$dry_run && exit 0

echo
if ((${#failures[@]} == 0)); then
    echo "All runs are OK. Log: $log"
    exit 0
fi
echo "${#failures[@]} runs are not OK:"
printf '  %s\n' "${failures[@]}"
echo "Log: $log"
exit 1

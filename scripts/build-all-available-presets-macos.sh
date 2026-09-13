#!/bin/sh
set -u

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
root_dir="$(CDPATH= cd -- "$script_dir/.." && pwd)"

mkdir -p "$root_dir/build"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "This script is intended to run on macOS (uname: $(uname -s))." >&2
  exit 2
fi

cmd="python3 $root_dir/scripts/build-all-available-presets.py --require-os Darwin"
if [ "${1:-}" = "--run-tests" ]; then
  cmd="$cmd --run-tests"
fi

exec sh -c "$cmd"

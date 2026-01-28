#!/usr/bin/env bash
#
# File: smoke-tests.sh
#
# Runs basic "smoke" tests to verify that a generated `sil` binary works as
# expected.
#
# Only Linux and (non-Cocoa) macOS builds are currently supported.

set -euo pipefail

# shellcheck disable=SC2155
readonly SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
# shellcheck disable=SC2155
readonly PROJECT_DIR=$(readlink -f "$SCRIPT_DIR/..")
readonly VERSION_FILE="$PROJECT_DIR/Sil-Q-version.txt"
readonly SIL_BINARY="$PROJECT_DIR/sil"

###
### Test 1: Check the version + whether the binary can run at all
###

# Get expected version from the version file.
expected_version=$(grep '^Sil-Q version:' "$VERSION_FILE" | sed 's/^Sil-Q version:[[:space:]]*//')
if [[ -z "$expected_version" ]]; then
    echo "[ERROR] Could not read version from $VERSION_FILE" >&2
    exit 1
fi

# Get actual version by running sil and capturing its version output.
print_version_command="$SIL_BINARY -v"
actual_output=$(eval "$print_version_command")
actual_output=$("$SIL_BINARY" -v)
actual_version=${actual_output#"Sil-Q version "}

if [[ "$actual_version" != "$expected_version" ]]; then
    echo "[ERROR] Version mismatch" >&2
    echo -e "  Expected: $expected_version\t(from $VERSION_FILE)" >&2
    echo -e "  Actual:   $actual_version\t(from $print_version_command)" >&2
    exit 1
fi

# All tests passed.
echo "[SUCCESS] All tests passed"
exit 0

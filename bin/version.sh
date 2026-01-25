#!/usr/bin/env bash

set -uo pipefail

# shellcheck disable=SC2155
readonly SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
# shellcheck disable=SC2155
readonly PROJECT_DIR=$(readlink -f "$SCRIPT_DIR/..")
readonly VERSION_FILE="$PROJECT_DIR/Sil-Q-version.txt"

if ! command -v git &>/dev/null; then
    echo "ERROR: 'git' command not found"
    exit 1
fi

# shellcheck disable=SC2155
readonly GIT_CURRENT_HASH="$(git rev-parse --short HEAD)"
# shellcheck disable=SC2155
readonly TIMESTAMP_UTC="$(date -u +'%Y-%m-%dT%H:%M:%S+00:00')"

echo "== Updating version file '$VERSION_FILE' =="
echo "git commit: $GIT_CURRENT_HASH" >>"$VERSION_FILE" || exit 1
echo "build generated at: $TIMESTAMP_UTC" >>"$VERSION_FILE" || exit 1

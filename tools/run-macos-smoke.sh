#!/usr/bin/env bash
set -euo pipefail
[[ $# == 2 ]] || { echo 'usage: run-macos-smoke.sh <publish-directory> <rid>' >&2; exit 1; }
root=$(cd "$1" && pwd)
case "$2:$(uname -m)" in
    osx-arm64:arm64|osx-x64:x86_64) ;;
    *) echo "error: $2 must be executed on its matching native runner" >&2; exit 1 ;;
esac
# A fresh HOME and unrelated cwd exercise a first launch without touching a
# developer's settings. A native deadlock must fail CI instead of hanging it.
python3 - "$root/FruityPrime" <<'PY'
import os
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix="fruity-smoke-") as temp:
    env = dict(os.environ, HOME=temp, DOTNET_CLI_HOME=temp)
    result = subprocess.run([sys.argv[1], "-smoketest"], cwd=temp, env=env, timeout=120)
    sys.exit(result.returncode)
PY

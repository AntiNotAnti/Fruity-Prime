#!/usr/bin/env bash
set -euo pipefail

# Sign every Mach-O shipped beside FruityPrime, then the apphost itself with
# the entitlement the .NET JIT needs under hardened runtime. This is run on a
# real Mac on purpose: cross-publishing from Linux can produce Mach-O files,
# but Linux cannot give them a macOS code signature or verify the result.
#
# Usage:
#   bash tools/macos-sign.sh publish/osx-arm64 arm64
#   bash tools/macos-sign.sh publish/osx-arm64 arm64 "Developer ID Application: ..."
#
# With no identity the build gets an ad-hoc signature. That is enough for CI,
# local builds and Apple-silicon's requirement that native code be signed. A
# release should pass a Developer ID identity and notarize the archive as well.

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "usage: $0 <publish-dir> <arm64|x86_64> [signing-identity]" >&2
    exit 2
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "macos-sign: this check must run on macOS" >&2
    exit 2
fi

publish_dir=$1
expected_arch=$2
identity=${3:--}

case "$expected_arch" in
    arm64|x86_64) ;;
    *)
        echo "macos-sign: expected architecture must be arm64 or x86_64" >&2
        exit 2
        ;;
esac

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
entitlements="$repo_root/tools/macos/FruityPrime.entitlements"
apphost="$publish_dir/FruityPrime"

if [[ ! -f "$apphost" ]]; then
    echo "macos-sign: missing $apphost" >&2
    exit 1
fi
if [[ ! -f "$entitlements" ]]; then
    echo "macos-sign: missing $entitlements" >&2
    exit 1
fi

# Collect the native files rather than keying only on extension. That catches
# the apphost plus any future native helper without accidentally feeding a
# managed assembly or data file to codesign.
native_files=()
while IFS= read -r -d '' candidate; do
    if file -b "$candidate" | grep -q 'Mach-O'; then
        native_files+=("$candidate")
    fi
done < <(find "$publish_dir" -maxdepth 1 -type f -print0)

if [[ ${#native_files[@]} -eq 0 ]]; then
    echo "macos-sign: no Mach-O files found in $publish_dir" >&2
    exit 1
fi

# A package named osx-arm64 that quietly contains one x64 dylib is every bit
# as broken as an x64 main binary. Check all of them before signing so the
# error says which file is wrong rather than surfacing later as dyld's SIGKILL.
for native in "${native_files[@]}"; do
    archs=$(lipo -archs "$native")
    if [[ " $archs " != *" $expected_arch "* ]]; then
        echo "macos-sign: $(basename "$native") is [$archs], expected $expected_arch" >&2
        exit 1
    fi
done

sign_args=(--force --sign "$identity" --options runtime)
if [[ "$identity" != "-" ]]; then
    # Developer ID signatures should carry a secure timestamp. Ad-hoc
    # signatures cannot be timestamped and intentionally omit this switch.
    sign_args+=(--timestamp)
fi

# Sign inside-out: the loose dylibs first, then the .NET apphost. The apphost
# gets allow-jit; native libraries do not need that entitlement themselves.
for native in "${native_files[@]}"; do
    if [[ "$native" == "$apphost" ]]; then
        continue
    fi
    codesign "${sign_args[@]}" "$native"
done
codesign "${sign_args[@]}" --entitlements "$entitlements" "$apphost"

# Verify every item because --deep is a bundle traversal and these native
# libraries intentionally live beside a command-line apphost, not in an .app.
for native in "${native_files[@]}"; do
    codesign --verify --strict --verbose=2 "$native"
done

# Keep the useful signature facts in the Actions log. For an ad-hoc CI build
# there is no Authority line; for a Developer ID release there is.
echo "macos-sign: signed ${#native_files[@]} Mach-O files for $expected_arch"
codesign -dvv "$apphost" 2>&1 \
    | grep -E '^(Identifier|Format|CodeDirectory|Authority|TeamIdentifier|Runtime Version)=' \
    || true

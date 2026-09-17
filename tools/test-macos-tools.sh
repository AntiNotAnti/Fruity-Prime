#!/usr/bin/env bash
# Negative regressions for the gates used by both workflows; no game assets.
set -euo pipefail
[[ $(uname -s) == Darwin ]] || { echo 'error: tests require macOS' >&2; exit 1; }
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
temp=$(mktemp -d)
trap 'rm -rf "$temp"' EXIT
case "$(uname -m)" in
    arm64) rid=osx-arm64; other=x86_64 ;;
    x86_64) rid=osx-x64; other=arm64 ;;
    *) exit 1 ;;
esac
root="$temp/publish with spaces"
mkdir -p "$root/nested"
printf 'int main(void) { return 0; }\n' > "$temp/main.c"
printf 'int native_probe(void) { return 42; }\n' > "$temp/native.c"
clang "$temp/main.c" -o "$root/FruityPrime"
clang -dynamiclib "$temp/native.c" -o "$root/libopenal.1.dylib"
clang -dynamiclib "$temp/native.c" -o "$root/nested/extensionless-native"
"$repo/tools/sign-macos.sh" "$root"
"$repo/tools/check-macos-build.sh" "$root" "$rid"
expect_failure() {
    if "$@" > "$temp/failure.log" 2>&1; then
        cat "$temp/failure.log"
        echo "error: unexpectedly accepted: $*" >&2
        exit 1
    fi
    cat "$temp/failure.log"
}
expect_failure "$repo/tools/check-macos-build.sh" "$root" unsupported
chmod -x "$root/FruityPrime"
expect_failure "$repo/tools/check-macos-build.sh" "$root" "$rid"
chmod +x "$root/FruityPrime"
mv "$root/libopenal.1.dylib" "$temp/openal"
expect_failure "$repo/tools/check-macos-build.sh" "$root" "$rid"
mv "$temp/openal" "$root/libopenal.1.dylib"
clang -arch "$other" -dynamiclib "$temp/native.c" -o "$root/nested/wrong.dylib"
codesign --force --sign - "$root/nested/wrong.dylib"
expect_failure "$repo/tools/check-macos-build.sh" "$root" "$rid"
rm "$root/nested/wrong.dylib"
codesign --remove-signature "$root/nested/extensionless-native"
expect_failure "$repo/tools/check-macos-build.sh" "$root" "$rid"
"$repo/tools/sign-macos.sh" "$root"
# A fresh executable proves the missing-entitlement case independently of
# any metadata preserved while replacing an existing code signature.
clang "$temp/main.c" -o "$root/NoJit"
codesign --force --sign - "$root/NoJit"
mv "$root/NoJit" "$root/FruityPrime"
expect_failure "$repo/tools/check-macos-build.sh" "$root" "$rid"
echo 'macOS signing gate regressions passed.'

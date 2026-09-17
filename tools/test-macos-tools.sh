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

# Reproduce the full app layout, including non-code map data. A flat publish
# can sign and launch successfully while its enclosing app cannot be signed.
fixture="$temp/package-input"
mkdir -p "$fixture/maps"
cp "$root/FruityPrime" "$root/libopenal.1.dylib" "$fixture/"
printf '{"Name":"PACKAGING TEST"}\n' > "$fixture/maps/fixture.json"
"$repo/tools/package-macos.sh" "$fixture" "$temp/dist" "$rid" 1.2.3
mkdir "$temp/unpacked"
tar -xzf "$temp/dist/FruityPrime-v1.2.3-$rid.tar.gz" -C "$temp/unpacked"
app="$temp/unpacked/Fruity Prime.app"
[[ -f "$app/Contents/Resources/maps/fixture.json" ]] || exit 1
[[ ! -e "$app/Contents/MacOS/maps" ]] || exit 1
codesign --verify --deep --strict "$app"
printf '\n' >> "$app/Contents/Resources/maps/fixture.json"
expect_failure codesign --verify --deep --strict "$app"
dotnet run --project "$repo/tools/platformtest/platformtest.csproj" -c Release
echo 'macOS bundle resource regressions passed.'

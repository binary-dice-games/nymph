#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# Gathers the release files of spec/07_release_pipeline/DESIGN.md section 3
# from the build artifacts into OUT, writes one SHA256SUMS over them, and
# fails if a file is missing or an unexpected one is there.
#
#   sh release-assemble.sh <artifacts dir> <version> <out dir>
#
# <artifacts dir> holds the downloaded artifacts, one folder each:
#   build-linux-x86_64/ build-linux-arm64/ build-windows-msys2/
#   build-macos-arm64/ build-macos-x86_64/   (each a release build's dist/)
#   build-macos-universal/                   (the universal .mcpb)
#   build-web/                               (claude/viewer.html, extension/*.zip)
#   source/                                  (the source tarball)

set -eu
artifacts=$1
version=$2
out=$3
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

fail() {
  echo "release assemble: $*" >&2
  exit 1
}
take() { # <source file> [<name in OUT>]
  [ -f "$1" ] || fail "missing $1"
  cp "$1" "$out/${2:-$(basename "$1")}"
}

rm -rf "$out"
mkdir -p "$out"
v=$version

for platform in linux-x86_64 linux-arm64; do
  take "$artifacts/build-$platform/nymph-$v-$platform"
  take "$artifacts/build-$platform/nymph-$v-$platform.mcpb"
done
take "$artifacts/build-windows-msys2/nymph-$v-windows-x86_64.exe"
take "$artifacts/build-windows-msys2/nymph-$v-windows-x86_64.mcpb"
# Per-architecture macOS bundles are not published: the universal one is.
take "$artifacts/build-macos-arm64/nymph-$v-macos-arm64"
take "$artifacts/build-macos-x86_64/nymph-$v-macos-x86_64"
take "$artifacts/build-macos-universal/nymph-$v-macos-universal.mcpb"
take "$artifacts/build-linux-x86_64/nymph-skill-$v.zip"
take "$artifacts/build-web/extension/nymph-extension-$v.zip"
take "$artifacts/build-web/extension/nymph-extension-firefox-$v.zip"
take "$artifacts/build-web/claude/viewer.html" "nymph-mcp-app-$v.html"
take "$artifacts/source/nymph-$v-src.tar.gz"
take "$root/packaging/install/install.sh"
take "$root/packaging/install/install.ps1"
chmod +x "$out"/nymph-"$v"-linux-* "$out"/nymph-"$v"-macos-arm64 "$out"/nymph-"$v"-macos-x86_64

cmake -DDIR="$out" -P "$root/cmake/NymphChecksums.cmake"

expected="SHA256SUMS
install.ps1
install.sh
nymph-$v-linux-arm64
nymph-$v-linux-arm64.mcpb
nymph-$v-linux-x86_64
nymph-$v-linux-x86_64.mcpb
nymph-$v-macos-arm64
nymph-$v-macos-universal.mcpb
nymph-$v-macos-x86_64
nymph-$v-src.tar.gz
nymph-$v-windows-x86_64.exe
nymph-$v-windows-x86_64.mcpb
nymph-extension-$v.zip
nymph-extension-firefox-$v.zip
nymph-mcp-app-$v.html
nymph-skill-$v.zip"
actual=$(cd "$out" && LC_ALL=C ls -1)
[ "$actual" = "$(printf '%s\n' "$expected" | LC_ALL=C sort)" ] || fail "unexpected file list:
$actual"
(cd "$out" && cat SHA256SUMS)
echo "release assemble: $(echo "$expected" | wc -l | tr -d ' ') files in $out"

#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# Writes the npm package @binary-dice-games/nymph and the MCP Registry
# server.json of a release (spec/07_release_pipeline/DESIGN.md section
# 4.11) from packaging/npm/ and packaging/mcp-registry/, pinning the
# SHA-256 of the release executables in the release's SHA256SUMS.
#
#   sh npm-package.sh <version> <SHA256SUMS> <out dir>
#
# Writes <out dir>/package/ (what `npm publish` takes) and
# <out dir>/server.json (what `mcp-publisher publish` takes). Fails when
# the SHA256SUMS names no executable of <version>.

set -eu
version=$1
sums=$2
out=$3
root=$(cd "$(dirname "$0")/../.." && pwd)

fail() {
  echo "npm package: $*" >&2
  exit 1
}
printf '%s' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || fail "version $version is not X.Y.Z"
[ -f "$sums" ] || fail "missing $sums"

rm -rf "$out"
mkdir -p "$out/package/bin"
package=$out/package

# The executables only, as the plugin's checksums.txt (cmake/NymphPluginSync.cmake).
{
  echo "# SHA-256 of the nymph $version release executables (.github/scripts/npm-package.sh)."
  grep -E "^[0-9a-f]{64}  nymph-$version-[a-z]+-[a-z0-9_]+(\.exe)?\$" "$sums" || true
} > "$package/checksums.txt"
count=$(grep -c -v '^#' "$package/checksums.txt" || true)
[ "$count" -gt 0 ] || fail "no nymph-$version executable in $sums"

cp "$root/packaging/npm/bin/nymph.js" "$package/bin/nymph.js"
chmod +x "$package/bin/nymph.js"
cp "$root/packaging/npm/README.md" "$package/README.md"
cp "$root/LICENSE" "$package/LICENSE"
sed "s|@NYMPH_VERSION@|$version|g" "$root/packaging/npm/package.json.in" > "$package/package.json"
sed "s|@NYMPH_VERSION@|$version|g" "$root/packaging/mcp-registry/server.json.in" > "$out/server.json"
if grep -q '@[A-Z_]*@' "$package/package.json" "$out/server.json"; then
  fail "unfilled placeholder in $package/package.json or $out/server.json"
fi
echo "npm package: $package (nymph $version, $count executables pinned)"
echo "npm package: $out/server.json"

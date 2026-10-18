#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# Writes the Homebrew formula of a release from packaging/homebrew/nymph.rb.in
# (spec/07_release_pipeline/DESIGN.md section 4.6), with the SHA-256s of the
# release's SHA256SUMS.
#
#   sh homebrew-formula.sh <version> <SHA256SUMS> <base URL> <out nymph.rb>
#
# <base URL> is where the release's files are: the GitHub release download
# URL for the published formula, or file://<folder> to test it before.

set -eu
version=$1
sums=$2
base=${3%/}
out=$4
root=$(cd "$(dirname "$0")/../.." && pwd)

sha_of() {
  hash=$(awk -v name="$1" '$2 == name { print $1 }' "$sums")
  [ -n "$hash" ] || { echo "homebrew formula: $1 is not in $sums" >&2; exit 1; }
  echo "$hash"
}
source_name=nymph-$version-src.tar.gz
app_name=nymph-mcp-app-$version.html
source_sha=$(sha_of "$source_name")
app_sha=$(sha_of "$app_name")

sed -e "s|@SOURCE_URL@|$base/$source_name|" \
    -e "s|@SOURCE_SHA256@|$source_sha|" \
    -e "s|@APP_URL@|$base/$app_name|" \
    -e "s|@APP_SHA256@|$app_sha|" \
    "$root/packaging/homebrew/nymph.rb.in" > "$out"
if grep -q '@[A-Z_]*@' "$out"; then
  echo "homebrew formula: unfilled placeholder in $out" >&2
  exit 1
fi
echo "homebrew formula: $out (nymph $version)"

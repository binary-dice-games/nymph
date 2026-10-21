#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# Writes the winget manifests of a release from packaging/winget/*.yaml.in
# (spec/07_release_pipeline/DESIGN.md section 4.10), with the SHA-256 of the
# Windows executable in the release's SHA256SUMS.
#
#   sh winget-manifest.sh <version> <SHA256SUMS> <base URL> <out dir>
#
# <base URL> is where the release's files are (the GitHub release download
# URL). The three files, BinaryDiceGames.Nymph.yaml, .installer.yaml and
# .locale.en-US.yaml, are written to <out dir>.

set -eu
version=$1
sums=$2
base=${3%/}
outdir=$4
root=$(cd "$(dirname "$0")/../.." && pwd)

exe_name=nymph-$version-windows-x86_64.exe
hash=$(awk -v name="$exe_name" '$2 == name { print toupper($1) }' "$sums")
[ -n "$hash" ] || { echo "winget manifest: $exe_name is not in $sums" >&2; exit 1; }

# <base>/<tag> -> the release page, for the release notes link.
release_url=$(echo "$base" | sed 's|/releases/download/|/releases/tag/|')

mkdir -p "$outdir"
for template in "$root"/packaging/winget/*.yaml.in; do
  name=$(basename "$template" .in)
  sed -e "s|@VERSION@|$version|" \
      -e "s|@INSTALLER_URL@|$base/$exe_name|" \
      -e "s|@INSTALLER_SHA256@|$hash|" \
      -e "s|@RELEASE_URL@|$release_url|" \
      "$template" > "$outdir/$name"
  if grep -q '@[A-Z_0-9]*@' "$outdir/$name"; then
    echo "winget manifest: unfilled placeholder in $outdir/$name" >&2
    exit 1
  fi
done
echo "winget manifest: $outdir (nymph $version)"

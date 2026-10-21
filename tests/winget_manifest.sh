#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# spec/07_release_pipeline/DESIGN.md section 4.10: .github/scripts/winget-manifest.sh
# fills the three winget manifests from a SHA256SUMS: version, URL, upper-case
# SHA-256 of the Windows executable only; fails when the executable is not listed.
#
#   winget_manifest.sh <winget-manifest.sh> <work dir>
# Conditions single-quoted: check() evaluates them.
# shellcheck disable=SC2016,SC2034
set -u
script=$1
work=$2
failures=0

check() { # <description> <shell condition, evaluated>
  description=$1
  if eval "$2"; then
    echo "ok $description"
  else
    echo "FAIL $description"
    failures=$((failures + 1))
  fi
}

rm -rf "$work"
mkdir -p "$work"
base=https://github.com/binary-dice-games/nymph/releases/download/v1.2.3
cat > "$work/SHA256SUMS" <<EOF
aaaa1111  nymph-1.2.3-linux-x86_64
bbbb2222  nymph-1.2.3-windows-x86_64.exe
cccc3333  nymph-1.2.3-src.tar.gz
EOF

out="$work/out"
check 'script succeeds' 'sh "$script" 1.2.3 "$work/SHA256SUMS" "$base/" "$out" > "$work/log"'
installer="$out/BinaryDiceGames.Nymph.installer.yaml"
check 'three manifests written' '[ -f "$out/BinaryDiceGames.Nymph.yaml" ] && [ -f "$installer" ] && [ -f "$out/BinaryDiceGames.Nymph.locale.en-US.yaml" ]'
check 'installer URL is the Windows executable' 'grep -q "InstallerUrl: $base/nymph-1.2.3-windows-x86_64.exe\$" "$installer"'
check 'installer SHA-256 is upper case and the Windows one' 'grep -q "InstallerSha256: BBBB2222\$" "$installer"'
check 'portable, with the nymph command' 'grep -q "InstallerType: portable" "$installer" && grep -q "^  - nymph\$" "$installer"'
check 'version in all three' '[ "$(cat "$out"/*.yaml | grep -c "^PackageVersion: 1.2.3\$")" = 3 ]'
check 'release notes link is the release page' 'grep -q "ReleaseNotesUrl: https://github.com/binary-dice-games/nymph/releases/tag/v1.2.3\$" "$out/BinaryDiceGames.Nymph.locale.en-US.yaml"'
check 'no placeholder left' '! grep -q "@[A-Z_0-9]*@" "$out"/*.yaml'

grep -v windows "$work/SHA256SUMS" > "$work/SHA256SUMS.nowin"
check 'missing executable fails' '! sh "$script" 1.2.3 "$work/SHA256SUMS.nowin" "$base" "$work/out2" 2> "$work/err" && grep -q "is not in" "$work/err"'

[ "$failures" = 0 ]

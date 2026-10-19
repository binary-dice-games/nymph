#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# spec/07_release_pipeline/PLAN.md step 3: packaging/install/install.sh in
# a scratch HOME (never the real one). The PATH step: one block per start-up
# file, nothing twice, nothing for a folder already on PATH, nothing with
# NYMPH_NO_MODIFY_PATH=1 or as root, and --uninstall restores the files.
# The release install: download from a fake release folder, SHA-256 check,
# install, PATH; a tampered file is never installed.
#
#   sh install_script.sh <install.sh> <work dir>
#
# Exits 77 (CTest's skip) without curl, which file:// downloads need.

# Conditions are single-quoted: check() evaluates them.
# shellcheck disable=SC2016,SC2034
set -u
script=$1
work=$2

failures=0
check() { # <description> <shell condition, evaluated>
  description=$1
  if eval "$2"; then
    echo "ok   $description"
  else
    echo "FAIL $description"
    failures=$((failures + 1))
  fi
}
count_blocks() { # <file>: number of nymph blocks
  if [ -f "$1" ]; then grep -cxF '# >>> nymph >>>' "$1"; else echo 0; fi
}
same() { cmp -s "$1" "$2"; }
has() { echo "$1" | grep -q -- "$2"; } # <text> <pattern>

rm -rf "$work"
mkdir -p "$work"
home=$work/home
bin=$work/bin
base_path=/usr/bin:/bin
fresh_home() {
  rm -rf "$home"
  mkdir -p "$home"
}
# Runs install.sh as a non-root user with a scratch HOME and a known PATH.
run() { # [VAR=value...] -- args
  vars=""
  while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    vars="$vars $1"
    shift
  done
  shift
  # shellcheck disable=SC2086
  env -i PATH="$base_path" HOME="$home" NYMPH_INSTALL_UID=1000 $vars sh "$script" "$@"
}

# ── PATH: bash user with an existing .bashrc ─────────────────────────────────

fresh_home
printf 'alias ll="ls -l"\n' > "$home/.bashrc"
cp "$home/.bashrc" "$work/bashrc.orig"
out=$(run SHELL=/bin/bash -- --path-only "$bin")
check "bash: block in .profile and .bashrc" '[ "$(count_blocks "$home/.profile")" = 1 ] && [ "$(count_blocks "$home/.bashrc")" = 1 ]'
check "bash: no zsh file" '[ ! -e "$home/.zshenv" ]'
check "bash: says to open a new console" 'has "$out" "new console"'
check "bash: a login shell finds the folder" \
  '[ "$(env -i HOME="$home" PATH="$base_path" bash -lc "echo \$PATH" | cut -d: -f1)" = "$bin" ]'
run SHELL=/bin/bash -- --path-only "$bin" > /dev/null
check "bash: a second run adds nothing" '[ "$(count_blocks "$home/.profile")" = 1 ] && [ "$(count_blocks "$home/.bashrc")" = 1 ]'
run SHELL=/bin/bash -- --path-only "$work/other" > /dev/null
check "bash: another folder replaces the block" '[ "$(count_blocks "$home/.bashrc")" = 1 ] && grep -q "$work/other" "$home/.bashrc" && ! grep -q "\"$bin:" "$home/.bashrc"'
run SHELL=/bin/bash -- --path-only "$bin" > /dev/null
run SHELL=/bin/bash -- --uninstall --bin-dir "$bin" > /dev/null
check "bash: --uninstall restores .bashrc byte for byte" 'same "$home/.bashrc" "$work/bashrc.orig"'
check "bash: --uninstall removes the .profile it created" '[ ! -e "$home/.profile" ]'

# ── PATH: bash user with a .bash_profile ─────────────────────────────────────

fresh_home
printf '. ~/.bashrc\n' > "$home/.bash_profile"
run SHELL=/bin/bash -- --path-only "$bin" > /dev/null
check "bash_profile: gets the block too" '[ "$(count_blocks "$home/.bash_profile")" = 1 ]'

# ── PATH: zsh user ───────────────────────────────────────────────────────────

fresh_home
run SHELL=/usr/bin/zsh -- --path-only "$bin" > /dev/null
check "zsh: block in .zshenv and .profile" '[ "$(count_blocks "$home/.zshenv")" = 1 ] && [ "$(count_blocks "$home/.profile")" = 1 ]'
check "zsh: no .bashrc" '[ ! -e "$home/.bashrc" ]'
if command -v zsh > /dev/null 2>&1; then
  check "zsh: a new zsh finds the folder" \
    '[ "$(env -i HOME="$home" PATH="$base_path" zsh -c "echo \$PATH" | cut -d: -f1)" = "$bin" ]'
fi
mkdir -p "$home/zdot"
fresh_home
mkdir -p "$home/zdot"
run SHELL=/bin/zsh ZDOTDIR="$home/zdot" -- --path-only "$bin" > /dev/null
check "zsh: ZDOTDIR is honoured" '[ "$(count_blocks "$home/zdot/.zshenv")" = 1 ]'

# ── PATH: sh user, fish user ─────────────────────────────────────────────────

fresh_home
run SHELL=/bin/sh -- --path-only "$bin" > /dev/null
check "sh: block in .profile only" '[ "$(count_blocks "$home/.profile")" = 1 ] && [ ! -e "$home/.bashrc" ] && [ ! -e "$home/.zshenv" ]'
check "sh: .profile puts the folder first" \
  '[ "$(env -i HOME="$home" PATH="$base_path" sh -c ". \$HOME/.profile; echo \$PATH" | cut -d: -f1)" = "$bin" ]'

fish_path=$(command -v fish || true)
if [ -n "$fish_path" ]; then
  fresh_home
  base_path="$(dirname "$fish_path"):/usr/bin:/bin"
  run SHELL="$fish_path" XDG_CONFIG_HOME="$home/.config" -- --path-only "$bin" > /dev/null
  in_fish() { env -i HOME="$home" XDG_CONFIG_HOME="$home/.config" PATH="$base_path" \
    fish -c 'set -e -g fish_user_paths 2>/dev/null; contains -- $argv[1] $fish_user_paths' "$bin"; }
  check "fish: fish_user_paths has the folder" 'in_fish'
  run SHELL="$fish_path" XDG_CONFIG_HOME="$home/.config" -- --uninstall --bin-dir "$bin" > /dev/null
  check "fish: --uninstall removes it" '! in_fish'
  base_path=/usr/bin:/bin
fi

# ── PATH: nothing to do, or not allowed to ───────────────────────────────────

fresh_home
out=$(env -i PATH="$bin:$base_path" HOME="$home" NYMPH_INSTALL_UID=1000 SHELL=/bin/bash sh "$script" --path-only "$bin/")
check "already on PATH: no file written" '[ -z "$(ls -A "$home")" ]'
check "already on PATH: says so" 'has "$out" "already on PATH"'
out=$(run SHELL=/bin/bash NYMPH_NO_MODIFY_PATH=1 -- --path-only "$bin")
check "NYMPH_NO_MODIFY_PATH=1: no file written, line printed" '[ -z "$(ls -A "$home")" ] && has "$out" "export PATH=\"$bin:"'
out=$(env -i PATH="$base_path" HOME="$home" NYMPH_INSTALL_UID=0 SHELL=/bin/bash sh "$script" --path-only "$bin")
check "root: no file written, line printed" '[ -z "$(ls -A "$home")" ] && has "$out" "root"'

# ── Release install ──────────────────────────────────────────────────────────

if ! command -v curl > /dev/null 2>&1; then
  echo "no curl: release install checks skipped"
  [ $failures -eq 0 ] || exit 1
  exit 77
fi
release=$work/release
mkdir -p "$release"
printf '#!/bin/sh\necho "fake nymph $*"\n' > "$work/fake"
: > "$release/SHA256SUMS"
for platform in linux-x86_64 linux-arm64 macos-x86_64 macos-arm64 windows-x86_64.exe; do
  cp "$work/fake" "$release/nymph-9.8.7-$platform"
  echo "$(sha256sum "$work/fake" | cut -d ' ' -f 1)  nymph-9.8.7-$platform" >> "$release/SHA256SUMS"
done
fresh_home
out=$(run SHELL=/bin/bash NYMPH_RELEASE_URL="file://$release" -- --bin-dir "$bin")
check "install: the executable is in the folder" '[ -x "$bin/nymph" ] && [ "$("$bin/nymph" --version)" = "fake nymph --version" ]'
check "install: and on PATH" '[ "$(count_blocks "$home/.profile")" = 1 ]'
check "install: names the release file" 'has "$out" "nymph-9.8.7-"'
rm -rf "$bin"
fresh_home
# A backslash in the download folder (as in a Windows path): GNU sha256sum
# then starts its line with a backslash, which must not end up in the hash.
mkdir -p "$work/tmp\\back"
run SHELL=/bin/bash TMPDIR="$work/tmp\\back" NYMPH_RELEASE_URL="file://$release" -- --bin-dir "$bin" > /dev/null
check "install: a backslash in the download path does not break the check" '[ -x "$bin/nymph" ]'
rm -rf "$bin"
fresh_home
run SHELL=/bin/bash NYMPH_RELEASE_URL="file://$release" -- --version 9.8.7 > /dev/null
check "install: default folder ~/.local/bin" '[ -x "$home/.local/bin/nymph" ]'
run SHELL=/bin/bash NYMPH_RELEASE_URL="file://$release" -- --version 1.0.0 > /dev/null 2>&1
code=$?
check "install: an unknown version fails" '[ $code -eq 1 ]'

for name in "$release"/nymph-9.8.7-*; do
  echo 'echo evil' >> "$name"
done
rm -rf "$bin"
out=$(run SHELL=/bin/bash NYMPH_RELEASE_URL="file://$release" -- --bin-dir "$bin" 2>&1)
code=$?
check "tampered release: exit 1, nothing installed" '[ $code -eq 1 ] && [ ! -e "$bin/nymph" ]'
check "tampered release: says why" 'has "$out" "not installed"'

run SHELL=/bin/bash -- --uninstall --bin-dir "$home/.local/bin" > /dev/null
check "uninstall: removes the executable and the PATH blocks" '[ ! -e "$home/.local/bin/nymph" ] && [ ! -e "$home/.profile" ]'

[ $failures -eq 0 ] || exit 1
echo "install.sh: all checks passed"

#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# nymph installer for Linux and macOS (also MSYS2 / Git Bash shells)
# (spec/07_release_pipeline/DESIGN.md section 4.9).
#
#   curl -fsSL https://github.com/binary-dice-games/nymph/releases/latest/download/install.sh | sh
#   sh install.sh [--version X.Y.Z] [--bin-dir DIR]   download and install the release executable
#   sh install.sh --path-only DIR                     only put DIR on PATH (cmake --install runs this)
#   sh install.sh --uninstall [--bin-dir DIR]         remove DIR/nymph and its PATH entry
#
# Installing downloads SHA256SUMS and the executable for this OS and CPU
# from the release (NYMPH_RELEASE_URL overrides the download base: a URL or
# file:// folder), checks the SHA-256, copies it to DIR/nymph (default
# ~/.local/bin, or <prefix>/bin when this script is <prefix>/share/nymph/
# install.sh) and puts DIR on PATH.
#
# PATH: nothing changes when DIR is already on PATH. Otherwise a block
# between "# >>> nymph >>>" and "# <<< nymph <<<" that prepends DIR is
# written to ~/.profile, ~/.bash_profile (when it exists), ~/.bashrc (bash
# users, or when it exists) and $ZDOTDIR/.zshenv (zsh users, or when
# ~/.zshrc exists); fish users get `fish_add_path -U DIR`. Writing twice
# changes nothing. As root, or with NYMPH_NO_MODIFY_PATH=1, nothing is
# written: the line to add is printed.
#
# Any failure prints one line on standard error and exits 1; a file whose
# SHA-256 differs is never installed.

# Single quotes keep $PATH and fish's $argv for the shell that reads them.
# shellcheck disable=SC2016
set -eu

fail() {
  echo "nymph install: $*" >&2
  exit 1
}

say() {
  echo "nymph install: $*"
}

begin_marker='# >>> nymph >>>'
end_marker='# <<< nymph <<<'

# ── PATH ──────────────────────────────────────────────────────────────────────

# Whether $1 is a folder of $PATH (ignoring a trailing slash).
on_path() {
  wanted=${1%/}
  old_ifs=$IFS
  IFS=:
  for entry in $PATH; do
    if [ "${entry%/}" = "$wanted" ]; then
      IFS=$old_ifs
      return 0
    fi
  done
  IFS=$old_ifs
  return 1
}

# Prints file $1 without the nymph block.
without_block() {
  awk -v b="$begin_marker" -v e="$end_marker" '
    $0 == b { skip = 1; next }
    $0 == e { skip = 0; next }
    !skip { print }' "$1"
}

# Prints standard input without its trailing blank lines.
strip_trailing_blank() {
  awk '/^[ \t]*$/ { blank = blank $0 "\n"; next } { printf "%s", blank; blank = ""; print }'
}

# Writes the block for folder $2 into file $1, replacing an older one.
# Prints the file name when the file changed.
write_block() {
  file=$1
  block=$(printf '%s\nexport PATH="%s:$PATH"\n%s' "$begin_marker" "$2" "$end_marker")
  if [ -f "$file" ]; then
    current=$(cat "$file")
    case $current in
      *"$block"*) return 0 ;;
    esac
    temp=$file.nymph.$$
    without_block "$file" | strip_trailing_blank > "$temp"
    if [ -s "$temp" ]; then
      printf '\n%s\n' "$block" >> "$temp"
    else
      printf '%s\n' "$block" > "$temp"
    fi
    cat "$temp" > "$file"
    rm -f "$temp"
  else
    printf '%s\n' "$block" > "$file"
  fi
  echo "$file"
}

# Removes the block from file $1; prints the file name when it changed.
remove_block() {
  file=$1
  [ -f "$file" ] || return 0
  grep -qxF "$begin_marker" "$file" || return 0
  temp=$file.nymph.$$
  # Also drops the blank line write_block added before the block.
  without_block "$file" | strip_trailing_blank > "$temp"
  if [ -s "$temp" ]; then
    cat "$temp" > "$file"
  else
    rm -f "$file"
  fi
  rm -f "$temp"
  echo "$file"
}

shell_name() {
  basename "${SHELL:-sh}"
}

# The shell start-up files that get the block, one per line.
profile_files() {
  shell=$(shell_name)
  echo "$HOME/.profile"
  [ -f "$HOME/.bash_profile" ] && echo "$HOME/.bash_profile"
  if [ "$shell" = bash ] || [ -f "$HOME/.bashrc" ]; then
    echo "$HOME/.bashrc"
  fi
  if [ "$shell" = zsh ] || [ -f "${ZDOTDIR:-$HOME}/.zshrc" ]; then
    echo "${ZDOTDIR:-$HOME}/.zshenv"
  fi
  return 0
}

# MSYS2 / Git Bash: a Windows path (C:\x or C:/x) as the shell sees it.
posix_dir() {
  case $1 in
    [A-Za-z]:[\\/]*)
      if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$1"
        return 0
      fi
      ;;
  esac
  echo "$1"
}

add_to_path() {
  dir=$(posix_dir "$1")
  if on_path "$dir"; then
    say "$dir is already on PATH"
    return 0
  fi
  line="export PATH=\"$dir:\$PATH\""
  if [ "${NYMPH_NO_MODIFY_PATH:-}" = 1 ]; then
    say "NYMPH_NO_MODIFY_PATH=1: PATH not changed; add $dir to it: $line"
    return 0
  fi
  # NYMPH_INSTALL_UID: tests only, to run the user path as root.
  if [ "${NYMPH_INSTALL_UID:-$(id -u)}" = 0 ]; then
    say "running as root: no profile changed; each user adds $dir to PATH: $line"
    return 0
  fi
  changed=""
  if [ "$(shell_name)" = fish ] && command -v fish >/dev/null 2>&1; then
    # fish_add_path skips a folder that does not exist yet. A global
    # fish_user_paths (set by a system config) would hide the universal one.
    mkdir -p "$dir" || fail "cannot create $dir"
    fish -c 'set -e -g fish_user_paths 2>/dev/null; fish_add_path -U -- $argv[1]' "$dir" ||
      fail "fish_add_path failed for $dir"
    changed="fish universal fish_user_paths"
  fi
  files=$(profile_files)
  old_ifs=$IFS
  IFS='
'
  for file in $files; do
    written=$(write_block "$file" "$dir") || fail "cannot write $file"
    [ -z "$written" ] || changed="$changed${changed:+, }$written"
  done
  IFS=$old_ifs
  if [ -n "$changed" ]; then
    say "added $dir to PATH in $changed"
    say "open a new console to use nymph (or run: $line)"
  fi
}

remove_from_path() {
  dir=$(posix_dir "$1")
  changed=""
  if command -v fish >/dev/null 2>&1; then
    if fish -c 'set -e -g fish_user_paths 2>/dev/null; contains -- $argv[1] $fish_user_paths' "$dir" 2>/dev/null; then
      fish -c 'set -e -g fish_user_paths 2>/dev/null; set -U fish_user_paths (string match -v -- $argv[1] $fish_user_paths)' "$dir" || true
      changed="fish universal fish_user_paths"
    fi
  fi
  for file in "$HOME/.profile" "$HOME/.bash_profile" "$HOME/.bashrc" "${ZDOTDIR:-$HOME}/.zshenv"; do
    [ -f "$file" ] || continue
    removed=$(remove_block "$file") || fail "cannot write $file"
    [ -z "$removed" ] || changed="$changed${changed:+, }$removed"
  done
  if [ -n "$changed" ]; then
    say "removed nymph's PATH entry from $changed"
  fi
}

# ── Release download ──────────────────────────────────────────────────────────

sha256_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d ' ' -f 1
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | cut -d ' ' -f 1
  elif command -v openssl >/dev/null 2>&1; then
    openssl dgst -sha256 -r "$1" | cut -d ' ' -f 1
  else
    fail "no sha256sum, shasum or openssl to check the download"
  fi
}

download() { # <url> <file>
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL --retry 2 -o "$2" "$1" 2>/dev/null || fail "could not download $1"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O "$2" "$1" || fail "could not download $1"
  else
    fail "no curl or wget to download $1"
  fi
}

install_release() {
  case $(uname -s) in
    Linux) os=linux ext= ;;
    Darwin) os=macos ext= ;;
    MINGW* | MSYS* | CYGWIN*) os=windows ext=.exe ;;
    *) fail "no nymph release for this system ($(uname -s)); build it from source (docs/building.md)" ;;
  esac
  case $(uname -m) in
    x86_64 | amd64 | AMD64) arch=x86_64 ;;
    aarch64 | arm64 | ARM64) arch=arm64 ;;
    *) fail "no nymph release for this CPU ($(uname -m)); build it from source (docs/building.md)" ;;
  esac

  if [ -n "${NYMPH_RELEASE_URL:-}" ]; then
    base=$NYMPH_RELEASE_URL
  elif [ -n "$version" ]; then
    base=https://github.com/binary-dice-games/nymph/releases/download/v$version
  else
    base=https://github.com/binary-dice-games/nymph/releases/latest/download
  fi

  work=$(mktemp -d 2>/dev/null || echo "${TMPDIR:-/tmp}/nymph-install.$$")
  mkdir -p "$work"
  trap 'rm -rf "$work"' EXIT INT TERM
  download "$base/SHA256SUMS" "$work/SHA256SUMS"
  pattern="  nymph-${version:-[0-9][^ ]*}-$os-$arch$ext\$"
  line=$(grep -E "$pattern" "$work/SHA256SUMS" | head -n 1 || true)
  [ -n "$line" ] || fail "$base has no nymph executable for $os-$arch"
  expected=${line%% *}
  name=${line##* }
  download "$base/$name" "$work/$name"
  actual=$(sha256_of "$work/$name")
  [ "$actual" = "$expected" ] || fail "$base/$name has SHA-256 $actual, not $expected as SHA256SUMS says; not installed"

  mkdir -p "$bin_dir" || fail "cannot create $bin_dir"
  chmod +x "$work/$name"
  cp "$work/$name" "$bin_dir/nymph$ext.new.$$" || fail "cannot write $bin_dir"
  mv -f "$bin_dir/nymph$ext.new.$$" "$bin_dir/nymph$ext" || fail "cannot write $bin_dir/nymph$ext"
  say "installed $name as $bin_dir/nymph$ext"
  add_to_path "$bin_dir"
}

# ── Arguments ─────────────────────────────────────────────────────────────────

here=$(cd "$(dirname "$0")" 2>/dev/null && pwd || echo .)
case $here in
  */share/nymph) bin_dir=${here%/share/nymph}/bin ;;
  *) bin_dir=$HOME/.local/bin ;;
esac
version=${NYMPH_VERSION:-}
mode=install
path_dir=""
while [ $# -gt 0 ]; do
  case $1 in
    --version) [ $# -ge 2 ] || fail "--version needs a value"; version=${2#v}; shift 2 ;;
    --bin-dir) [ $# -ge 2 ] || fail "--bin-dir needs a value"; bin_dir=$2; shift 2 ;;
    --path-only) [ $# -ge 2 ] || fail "--path-only needs a folder"; mode=path; path_dir=$2; shift 2 ;;
    --uninstall) mode=uninstall; shift ;;
    -h | --help) sed -n '2,30p' "$0" 2>/dev/null | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) fail "unknown argument: $1 (see --help)" ;;
  esac
done

case $mode in
  path) add_to_path "$path_dir" ;;
  uninstall)
    for file in "$bin_dir/nymph" "$bin_dir/nymph.exe"; do
      if [ -f "$file" ]; then
        rm -f "$file" || fail "cannot remove $file"
        say "removed $file"
      fi
    done
    remove_from_path "$bin_dir"
    ;;
  install) install_release ;;
esac
